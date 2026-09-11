#include "Framework/ForeverRoadnetFrameworkComponent.h"

#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"

#include "map/map.h"
#include "map/geometry.h"

#include <unordered_set>

#define ROAD_WORLD_SCALE 1000.f
// 开口cube/路口mesh都铺在Z=0(地图单位)，和这一片"plain"地形的高度基本重合，会跟地形网格
// 发生共面z-fighting(和ForeverTerrainFrameworkComponent.cpp的HEIGHT_EPSILON是同一个问题)。
// 2mm(世界单位)第一次PIE验证时实测不够(仍然被地形盖住/看不见)，改成5cm验证有效后，
// 按用户要求收到1cm——地形本身按element为单位起伏，1cm相对10m一个格子的尺度依然可以
// 忽略不计，足够压过地形高度采样的误差范围又不会太明显地"浮空"。
#define ROADNET_HEIGHT_EPSILON 10.f

// 导航图debug可视化用的box/ribbon尺寸(世界单位)——纯debug标记，不追求精确物理尺寸，数值是
// 早前验证阶段调出来的经验值：一开始试过纯平面(单一Z高度)画node/edge，PIE里完全看不见
// (和开口/路口mesh最初的z-fighting是同一类问题，纯平面即使有ROADNET_HEIGHT_EPSILON也太薄)，
// 改成真正有厚度的3D box(node)/宽度的ribbon(edge)才终于稳定可见，厚度定在50(约5cm)。
#define NAV_DEBUG_HEIGHT 50.f
#define NAV_DEBUG_NODE_HALF_SIZE 40.f
#define NAV_DEBUG_EDGE_HALF_WIDTH 8.f

using namespace std;

namespace {
	// 双面四边形：不区分正反面，四个角点无论按什么环绕顺序传入，两个方向的三角形都会画一遍——
	// 导航debug mesh的box/ribbon朝向五花八门(任意角度的道路/任意方向的路口连接)，用这个技巧
	// 就不用为每个面单独推导"哪个环绕顺序才是正面朝上"，从任意角度看都不会因为背面剔除而消失。
	void AppendQuadDoubleSided(TArray<FVector>& vertices, TArray<int32>& triangles,
		const FVector& v00, const FVector& v10, const FVector& v11, const FVector& v01) {
		int32 base = vertices.Num();
		vertices.Add(v00); vertices.Add(v10); vertices.Add(v11); vertices.Add(v01);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 1);
		triangles.Add(base); triangles.Add(base + 3); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 1); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 3);
	}

	// 导航锚点用的小box：以center为中心、halfSize为水平半边长、[zBottom,zTop]为竖直范围的
	// 轴对齐长方体，6个面都用AppendQuadDoubleSided画，不需要关心每个面的外法线朝向。
	void AppendNavBox(TArray<FVector>& vertices, TArray<int32>& triangles,
		const FVector2D& center, float halfSize, float zBottom, float zTop) {
		FVector v000(center.X - halfSize, center.Y - halfSize, zBottom);
		FVector v100(center.X + halfSize, center.Y - halfSize, zBottom);
		FVector v110(center.X + halfSize, center.Y + halfSize, zBottom);
		FVector v010(center.X - halfSize, center.Y + halfSize, zBottom);
		FVector v001(center.X - halfSize, center.Y - halfSize, zTop);
		FVector v101(center.X + halfSize, center.Y - halfSize, zTop);
		FVector v111(center.X + halfSize, center.Y + halfSize, zTop);
		FVector v011(center.X - halfSize, center.Y + halfSize, zTop);

		AppendQuadDoubleSided(vertices, triangles, v001, v101, v111, v011); // 顶
		AppendQuadDoubleSided(vertices, triangles, v010, v110, v100, v000); // 底
		AppendQuadDoubleSided(vertices, triangles, v000, v100, v101, v001); // -Y
		AppendQuadDoubleSided(vertices, triangles, v100, v110, v111, v101); // +X
		AppendQuadDoubleSided(vertices, triangles, v110, v010, v011, v111); // +Y
		AppendQuadDoubleSided(vertices, triangles, v010, v000, v001, v011); // -X
	}

	// 导航连接用的细ribbon：from/to是两端锚点的世界坐标(各自的Z已经是该锚点box顶面的高度，
	// 见BuildNavGraphDebugMesh)，halfWidth是垂直于连线方向的半宽。
	void AppendNavEdgeRibbon(TArray<FVector>& vertices, TArray<int32>& triangles,
		const FVector& from, const FVector& to, float halfWidth) {
		FVector2D dir2D(to.X - from.X, to.Y - from.Y);
		float len = dir2D.Size();
		if (len < 1e-3f) return;
		dir2D /= len;
		FVector offset(-dir2D.Y * halfWidth, dir2D.X * halfWidth, 0.f);

		AppendQuadDoubleSided(vertices, triangles, from - offset, to - offset, to + offset, from + offset);
	}
}

UForeverRoadnetFrameworkComponent::UForeverRoadnetFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> roadPlainFinder(
		TEXT("/Game/Asset/Materials/RoadPlain.RoadPlain"));
	if (roadPlainFinder.Succeeded()) {
		roadPlainBaseMaterial = roadPlainFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> whiteFinder(
		TEXT("/Game/Asset/Materials/White.White"));
	if (whiteFinder.Succeeded()) {
		whiteBaseMaterial = whiteFinder.Object;
	}
}

void UForeverRoadnetFrameworkComponent::GenerateRoadnet(Map* inMap) {
	map = inMap;
	if (!map) return;

	AActor* owner = GetOwner();
	if (!owner) return;

	if (roadPlainBaseMaterial) {
		openingMaterial = UMaterialInstanceDynamic::Create(roadPlainBaseMaterial, this);
		junctionMaterial = UMaterialInstanceDynamic::Create(roadPlainBaseMaterial, this);
	}

	openingMesh = NewObject<UProceduralMeshComponent>(owner, TEXT("RoadOpenings"));
	openingMesh->SetupAttachment(owner->GetRootComponent());
	openingMesh->RegisterComponent();
	owner->AddInstanceComponent(openingMesh);

	junctionMesh = NewObject<UProceduralMeshComponent>(owner, TEXT("RoadJunctions"));
	junctionMesh->SetupAttachment(owner->GetRootComponent());
	junctionMesh->RegisterComponent();
	owner->AddInstanceComponent(junctionMesh);

	// 每条road在连着真实路口的那一端要收缩tiling范围，把面积让给路口mesh，避免路口处两条路
	// 的mesh互相重叠z-fighting——收缩距离必须用RoadJunction::Build里同一个approach.setback，
	// 不能自己另算一套，否则路面收缩量和路口mesh路缘角点的外移量对不上，要么留空隙要么重叠
	// (这正是第一版实现踩过的坑，见ForeverRoadnetFrameworkComponent.md)。
	TMap<Road*, float> trimAtStart, trimAtEnd;
	for (RoadJunction* junction : map->GetJunctions()) {
		for (const RoadJunctionApproach& ap : junction->GetApproaches()) {
			if (!ap.road) continue;
			if (ap.isStart) trimAtStart.Add(ap.road, ap.setback);
			else trimAtEnd.Add(ap.road, ap.setback);
		}
	}

	TArray<FVector> openingVertices;
	TArray<int32> openingTriangles;
	TArray<FVector2D> openingUvs;
	for (Road* road : map->GetRoads()) {
		float ts = trimAtStart.Contains(road) ? trimAtStart[road] : 0.f;
		float te = trimAtEnd.Contains(road) ? trimAtEnd[road] : 0.f;
		BuildRoadInstances(road, ts, te, openingVertices, openingTriangles, openingUvs);
		BuildOpeningMeshes(road, openingVertices, openingTriangles, openingUvs);
	}
	if (openingTriangles.Num() > 0) {
		openingMesh->CreateMeshSection(0, openingVertices, openingTriangles,
			TArray<FVector>(), openingUvs, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
		if (openingMaterial) openingMesh->SetMaterial(0, openingMaterial);
	}

	BuildJunctionMeshes();

	BuildPathRoadMeshes();

	BuildNavigationDebugMesh();
}

void UForeverRoadnetFrameworkComponent::BuildPathRoadMeshes() {
	if (!map) return;

	AActor* owner = GetOwner();
	if (!owner) return;

	UMaterialInterface* baseMaterial = roadPlainBaseMaterial;
	const string& materialPath = map->GetPathRoadMaterial();
	if (!materialPath.empty()) {
		// 运行时按字符串路径加载资产——和roadPlainBaseMaterial那种只能在构造函数里用的
		// ConstructorHelpers::FObjectFinder不是一回事，这里的路径来自RoadnetMod运行时数据，
		// 编译期不知道具体是哪个资产。
		UObject* loaded = StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, UTF8_TO_TCHAR(materialPath.c_str()));
		if (UMaterialInterface* loadedMaterial = Cast<UMaterialInterface>(loaded)) {
			baseMaterial = loadedMaterial;
		}
	}
	if (baseMaterial) {
		pathRoadMaterial = UMaterialInstanceDynamic::Create(baseMaterial, this);
	}

	if (!pathRoadMesh) {
		pathRoadMesh = NewObject<UProceduralMeshComponent>(owner, TEXT("PathRoads"));
		pathRoadMesh->SetupAttachment(owner->GetRootComponent());
		pathRoadMesh->RegisterComponent();
		owner->AddInstanceComponent(pathRoadMesh);
	}

	TArray<FVector> vertices;
	TArray<int32> triangles;
	for (Road* road : map->GetPathRoads()) {
		if (!road) continue;
		Node start = road->GetStart();
		Node end = road->GetEnd();
		float halfWidth = road->GetTotalWidth() * 0.5f * ROAD_WORLD_SCALE;

		FVector2D dir2D(end.GetX() - start.GetX(), end.GetY() - start.GetY());
		float len = dir2D.Size();
		if (len < 1e-3f) continue;
		dir2D /= len;
		FVector offset(-dir2D.Y * halfWidth, dir2D.X * halfWidth, 0.f);

		float z = ROADNET_HEIGHT_EPSILON;
		FVector from(start.GetX() * ROAD_WORLD_SCALE, start.GetY() * ROAD_WORLD_SCALE, z);
		FVector to(end.GetX() * ROAD_WORLD_SCALE, end.GetY() * ROAD_WORLD_SCALE, z);

		int32 base = vertices.Num();
		vertices.Add(from - offset); vertices.Add(to - offset); vertices.Add(to + offset); vertices.Add(from + offset);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 1);
		triangles.Add(base); triangles.Add(base + 3); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 1); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 3);
	}

	if (triangles.Num() > 0) {
		pathRoadMesh->CreateMeshSection(0, vertices, triangles,
			TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
		if (pathRoadMaterial) pathRoadMesh->SetMaterial(0, pathRoadMaterial);
	}
}

void UForeverRoadnetFrameworkComponent::BuildNavigationDebugMesh() {
	if (!map) return;

	AActor* owner = GetOwner();
	if (!owner) return;

	if (!vehicleNavMesh) {
		vehicleNavMesh = NewObject<UProceduralMeshComponent>(owner, TEXT("VehicleNavDebug"));
		vehicleNavMesh->SetupAttachment(owner->GetRootComponent());
		vehicleNavMesh->RegisterComponent();
		owner->AddInstanceComponent(vehicleNavMesh);
	}
	if (!pedestrianNavMesh) {
		pedestrianNavMesh = NewObject<UProceduralMeshComponent>(owner, TEXT("PedestrianNavDebug"));
		pedestrianNavMesh->SetupAttachment(owner->GetRootComponent());
		pedestrianNavMesh->RegisterComponent();
		owner->AddInstanceComponent(pedestrianNavMesh);
	}

	if (!bShowNavigationDebug) {
		// 关闭时清空已有section而不是整个跳过——避免"曾经打开过、现在关掉"时旧的可视化
		// mesh一直残留在场景里。
		vehicleNavMesh->ClearMeshSection(0);
		pedestrianNavMesh->ClearMeshSection(0);
		return;
	}

	if (whiteBaseMaterial && !vehicleNavMaterial) {
		vehicleNavMaterial = UMaterialInstanceDynamic::Create(whiteBaseMaterial, this);
	}
	if (roadPlainBaseMaterial && !pedestrianNavMaterial) {
		pedestrianNavMaterial = UMaterialInstanceDynamic::Create(roadPlainBaseMaterial, this);
	}

	BuildNavGraphDebugMesh(map->GetVehicleNavGraph(), vehicleNavMesh, vehicleNavMaterial);
	BuildNavGraphDebugMesh(map->GetPedestrianNavGraph(), pedestrianNavMesh, pedestrianNavMaterial);
}

void UForeverRoadnetFrameworkComponent::BuildNavGraphDebugMesh(
	const unordered_map<int, vector<pair<int, Connection*>>>& graph,
	UProceduralMeshComponent* mesh, UMaterialInstanceDynamic* material) {
	if (!mesh) return;

	TArray<FVector> vertices;
	TArray<int32> triangles;

	// 每条边的Connection*本身就带着两端锚点的真实Node(GetStart()/GetEnd())，不需要另外
	// 按id反查坐标——通过遍历所有边顺带拿到的两端坐标，用visitedNodes按id去重，同一个
	// 锚点被多条边引用时只画一次box，不会重叠堆叠。
	unordered_set<int> visitedNodes;
	for (const auto& [fromId, edges] : graph) {
		for (const auto& [toId, conn] : edges) {
			if (!conn) continue;
			Node start = conn->GetStart();
			Node end = conn->GetEnd();

			// box顶面高度=锚点自身真实Z(隧道场景下锚点可能已经在地下)+ROADNET_HEIGHT_EPSILON
			// (盖过地形/路面的z-fighting)+NAV_DEBUG_HEIGHT(box自身厚度)，ribbon直接贴在
			// 两端box的顶面高度上，让边看起来是从box顶接出去的，不会悬空或扎进box里。
			float fromZTop = start.GetZ() * ROAD_WORLD_SCALE + ROADNET_HEIGHT_EPSILON + NAV_DEBUG_HEIGHT;
			float toZTop = end.GetZ() * ROAD_WORLD_SCALE + ROADNET_HEIGHT_EPSILON + NAV_DEBUG_HEIGHT;
			FVector fromTop(start.GetX() * ROAD_WORLD_SCALE, start.GetY() * ROAD_WORLD_SCALE, fromZTop);
			FVector toTop(end.GetX() * ROAD_WORLD_SCALE, end.GetY() * ROAD_WORLD_SCALE, toZTop);
			AppendNavEdgeRibbon(vertices, triangles, fromTop, toTop, NAV_DEBUG_EDGE_HALF_WIDTH);

			if (visitedNodes.insert(fromId).second) {
				float zBottom = start.GetZ() * ROAD_WORLD_SCALE + ROADNET_HEIGHT_EPSILON;
				AppendNavBox(vertices, triangles, FVector2D(fromTop.X, fromTop.Y), NAV_DEBUG_NODE_HALF_SIZE, zBottom, zBottom + NAV_DEBUG_HEIGHT);
			}
			if (visitedNodes.insert(toId).second) {
				float zBottom = end.GetZ() * ROAD_WORLD_SCALE + ROADNET_HEIGHT_EPSILON;
				AppendNavBox(vertices, triangles, FVector2D(toTop.X, toTop.Y), NAV_DEBUG_NODE_HALF_SIZE, zBottom, zBottom + NAV_DEBUG_HEIGHT);
			}
		}
	}

	mesh->ClearMeshSection(0);
	if (triangles.Num() == 0) return;
	mesh->CreateMeshSection(0, vertices, triangles,
		TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	if (material) mesh->SetMaterial(0, material);
}

UInstancedStaticMeshComponent* UForeverRoadnetFrameworkComponent::GetOrCreateRoadISM(const string& meshPath) {
	if (meshPath.empty()) return nullptr;
	FString key = UTF8_TO_TCHAR(meshPath.c_str());

	if (auto* found = roadMeshInstances.Find(key)) {
		return *found;
	}

	AActor* owner = GetOwner();
	if (!owner) return nullptr;

	UStaticMesh* mesh = LoadObject<UStaticMesh>(nullptr, *key);
	if (!mesh) return nullptr;

	UInstancedStaticMeshComponent* ism = NewObject<UInstancedStaticMeshComponent>(owner, NAME_None, RF_Transient);
	ism->SetStaticMesh(mesh);
	ism->SetupAttachment(owner->GetRootComponent());
	ism->RegisterComponent();
	owner->AddInstanceComponent(ism);

	roadMeshInstances.Add(key, ism);
	return ism;
}

void UForeverRoadnetFrameworkComponent::BuildRoadInstances(Road* road, float trimStart, float trimEnd,
	TArray<FVector>& outVertices, TArray<int32>& outTriangles, TArray<FVector2D>& outUvs) {
	if (!road) return;

	float unit = road->GetUnit();
	if (unit <= 0.f) return;

	float totalLen = road->CalcDistance();
	if (totalLen <= 0.f) return;

	// 两端各收缩trimStart/trimEnd(地图单位)，把tiling范围限制在[tLow,tHigh]——收缩掉的那截
	// 面积交给路口mesh覆盖，两者不再重叠，见头文件注释。收缩量不超过总长的45%，避免两端
	// 收缩量之和意外超过整条路长度导致tLow>=tHigh。
	float tLow = FMath::Clamp(trimStart / totalLen, 0.f, 0.45f);
	float tHigh = 1.f - FMath::Clamp(trimEnd / totalLen, 0.f, 0.45f);
	if (tHigh <= tLow) return;

	UInstancedStaticMeshComponent* ism = GetOrCreateRoadISM(road->GetMesh());
	if (!ism) return;

	// 按每个开口把[tLow,tHigh]切成若干互不重叠的"保留区间"，每段各自独立铺tiling——这样铺出来
	// 的缺口精确等于开口自身的[t-halfFrac,t+halfFrac]范围(和BuildOpeningMeshes算cube长度用的
	// 是同一个halfFrac公式)，不会像第一版那样"按固定unit分段、只按段中点判断是否命中开口"，
	// 导致缺口宽度只能凑整到unit的倍数、和cube宽度对不上，见ForeverRoadnetFrameworkComponent.md。
	TArray<TPair<float, float>> keepRanges;
	keepRanges.Add(TPair<float, float>(tLow, tHigh));
	for (const RoadOpening& op : road->GetOpenings()) {
		float halfFrac = (op.width * 0.5f) / totalLen;
		float opStart = op.t - halfFrac;
		float opEnd = op.t + halfFrac;

		TArray<TPair<float, float>> next;
		for (const TPair<float, float>& range : keepRanges) {
			float rs = range.Key, re = range.Value;
			float os = FMath::Max(rs, opStart), oe = FMath::Min(re, opEnd);
			if (os >= oe) { next.Add(range); continue; }
			if (os > rs) next.Add(TPair<float, float>(rs, os));
			if (oe < re) next.Add(TPair<float, float>(oe, re));
		}
		keepRanges = next;
	}

	// 分段数量算法照抄老工程UMeshArray::RepeatMeshAlongCurve："scaledPrim落在[0.8,1.2]*primLen
	// 区间内"这条约束；因为Connection::GetPoint(f)本身已经是弧长参数化的(和
	// RepeatMeshAlongCurve内部自建的弧长表是同一套算法)，这里不需要再自己建一份弧长采样表，
	// 直接按t=rangeStart+(rangeEnd-rangeStart)*k/n取点即可得到该保留区间内的等弧长分段。
	auto tileRange = [&](float rangeStart, float rangeEnd) {
		float rangeLen = totalLen * (rangeEnd - rangeStart);
		if (rangeLen <= 0.f) return;

		int nLow = FMath::CeilToInt(rangeLen / (1.2f * unit));
		int nHigh = FMath::FloorToInt(rangeLen / (0.8f * unit));
		int n = (nLow <= nHigh && nLow > 0) ? FMath::Clamp(FMath::RoundToInt(rangeLen / unit), nLow, nHigh) : 0;
		if (n <= 0) {
			// 这一段(比如两个开口之间、或端点和第一个开口之间剩下的那一小截)连一节
			// default_x_x_x实例都铺不出来(短于0.8*unit)——不留空隙，退化成贴RoadPlain材质的
			// 扁平cube填满，和道路开口一样处理。
			float centerT = (rangeStart + rangeEnd) * 0.5f;
			AppendFlatRoadCube(road, centerT, rangeLen, road->GetTotalWidth(), outVertices, outTriangles, outUvs);
			return;
		}

		float actualSegLen = rangeLen / n;
		float scaleAlong = actualSegLen / unit;

		for (int k = 0; k < n; k++) {
			float t0 = rangeStart + (rangeEnd - rangeStart) * static_cast<float>(k) / n;
			float t1 = rangeStart + (rangeEnd - rangeStart) * static_cast<float>(k + 1) / n;
			float tMid = (t0 + t1) * 0.5f;

			Node start = road->GetPoint(t0);
			Node end = road->GetPoint(t1);
			float dx, dy, dz;
			road->GetTangent(tMid, dx, dy, dz);

			FVector startW(start.GetX() * ROAD_WORLD_SCALE, start.GetY() * ROAD_WORLD_SCALE, start.GetZ() * ROAD_WORLD_SCALE);
			FVector endW(end.GetX() * ROAD_WORLD_SCALE, end.GetY() * ROAD_WORLD_SCALE, end.GetZ() * ROAD_WORLD_SCALE);
			FVector mid = (startW + endW) * 0.5f;

			FVector fwd = FVector(dx, dy, dz).GetSafeNormal();
			if (fwd.IsNearlyZero()) fwd = FVector::ForwardVector;

			FTransform xform(fwd.Rotation(), mid, FVector(scaleAlong, 1.f, 1.f));
			ism->AddInstance(xform);
		}
		};

	for (const TPair<float, float>& range : keepRanges) {
		tileRange(range.Key, range.Value);
	}
}

void UForeverRoadnetFrameworkComponent::BuildOpeningMeshes(Road* road,
	TArray<FVector>& outVertices, TArray<int32>& outTriangles, TArray<FVector2D>& outUvs) {
	if (!road) return;
	const vector<RoadOpening>& openings = road->GetOpenings();
	if (openings.empty()) return;

	// 开口cube宽度取该road横断面总宽(两侧车行+停车+人行道加总)，覆盖整条路的宽度——
	// 要求5的简化做法：直接用一块贴RoadPlain材质的扁平cube代替原本该段的沿路重复mesh，
	// 不做"只挖开人行道/停车道"的精细几何裁剪，见ForeverRoadnetFrameworkComponent.md。
	float totalWidth = road->GetTotalWidth();

	// 同一个物理开口位置常常会有车行、人行各一条RoadOpening记录(Map::AddRoadAccessNode按
	// isVehicle分别调用、各自往road->openings里追加一条)，但视觉上只需要一块贴地cube盖住
	// 这段路面——两条记录的t/width如果一样，就是同一个物理开口，不去重的话会在完全相同的
	// 位置画两个完全重合的扁平quad，PIE里z-fighting闪烁。
	vector<float> drawnTs;
	for (const RoadOpening& op : openings) {
		bool alreadyDrawn = false;
		for (float t : drawnTs) {
			if (FMath::Abs(t - op.t) < 1e-4f) { alreadyDrawn = true; break; }
		}
		if (alreadyDrawn) continue;
		drawnTs.push_back(op.t);

		AppendFlatRoadCube(road, op.t, op.width, totalWidth, outVertices, outTriangles, outUvs);
	}
}

void UForeverRoadnetFrameworkComponent::AppendFlatRoadCube(Road* road, float centerT, float lengthAlongRoad, float crossWidth,
	TArray<FVector>& outVertices, TArray<int32>& outTriangles, TArray<FVector2D>& outUvs) {
	if (!road) return;
	if (crossWidth <= 0.f) crossWidth = 1.f;

	// 车道横断面现在以Connection连线为几何中心居中（见Source/Core/map/roadnet.md"车道居中"
	// 一节），下面按cx/cy±perp*halfWidth对称展开cube的写法因此正确——不需要再关心side0/side1
	// 具体怎么分配。
	Node center = road->GetPoint(centerT);
	float dx, dy, dz;
	road->GetTangent(centerT, dx, dy, dz);
	float len = FMath::Sqrt(dx * dx + dy * dy);
	if (len < 1e-6f) len = 1.f;
	float fwdX = dx / len, fwdY = dy / len;
	float perpX = fwdY, perpY = -fwdX;

	float halfLen = lengthAlongRoad * 0.5f;
	float halfWidth = crossWidth * 0.5f;
	float cx = center.GetX(), cy = center.GetY();

	FVector2D p00(cx - fwdX * halfLen - perpX * halfWidth, cy - fwdY * halfLen - perpY * halfWidth);
	FVector2D p10(cx + fwdX * halfLen - perpX * halfWidth, cy + fwdY * halfLen - perpY * halfWidth);
	FVector2D p11(cx + fwdX * halfLen + perpX * halfWidth, cy + fwdY * halfLen + perpY * halfWidth);
	FVector2D p01(cx - fwdX * halfLen + perpX * halfWidth, cy - fwdY * halfLen + perpY * halfWidth);

	int32 base = outVertices.Num();
	outVertices.Add(FVector(p00.X * ROAD_WORLD_SCALE, p00.Y * ROAD_WORLD_SCALE, ROADNET_HEIGHT_EPSILON));
	outVertices.Add(FVector(p10.X * ROAD_WORLD_SCALE, p10.Y * ROAD_WORLD_SCALE, ROADNET_HEIGHT_EPSILON));
	outVertices.Add(FVector(p11.X * ROAD_WORLD_SCALE, p11.Y * ROAD_WORLD_SCALE, ROADNET_HEIGHT_EPSILON));
	outVertices.Add(FVector(p01.X * ROAD_WORLD_SCALE, p01.Y * ROAD_WORLD_SCALE, ROADNET_HEIGHT_EPSILON));
	outUvs.Add(FVector2D(0, 0));
	outUvs.Add(FVector2D(1, 0));
	outUvs.Add(FVector2D(1, 1));
	outUvs.Add(FVector2D(0, 1));
	// 环绕顺序：对照ForeverTerrainFrameworkComponent::BuildLevel验证过的(v00,v11,v10)/(v00,v01,v11)
	// 模式，UE(左手坐标系)要求从上方看是"顺时针"(标准数学XY凸包意义下的负向面积)才是正面朝上；
	// 早期版本这里写反了(正面朝下，从上方完全看不到)，PIE验证后改正。
	outTriangles.Add(base); outTriangles.Add(base + 1); outTriangles.Add(base + 2);
	outTriangles.Add(base); outTriangles.Add(base + 2); outTriangles.Add(base + 3);
}

void UForeverRoadnetFrameworkComponent::BuildJunctionMeshes() {
	if (!map || !junctionMesh) return;

	TArray<FVector> vertices;
	TArray<int32> triangles;
	TArray<FVector2D> uvs;

	for (RoadJunction* junction : map->GetJunctions()) {
		const vector<RoadJunctionApproach>& approaches = junction->GetApproaches();
		if (approaches.size() < 2) continue;

		Intersection* node = junction->GetNode();
		if (!node) continue;

		// 中心点/curb点各自取该approach采样到的真实曲线高度(node->GetZ()/ap.curbZ，地图单位)，
		// 不再统一用ROADNET_HEIGHT_EPSILON这个平地假设——隧道场景下路口可能整体埋在地下
		// (TUNNEL_HEIGHT)，而curb点又各自沿不同road采样，彼此高度不一定相同(见roadnet.h
		// curbZ字段注释)，扇形三角化允许每个顶点独立取高度，不需要整个多边形共面。
		int32 centerIdx = vertices.Num();
		vertices.Add(FVector(node->GetX() * ROAD_WORLD_SCALE, node->GetY() * ROAD_WORLD_SCALE, node->GetZ() * ROAD_WORLD_SCALE + ROADNET_HEIGHT_EPSILON));
		uvs.Add(FVector2D(0.5f, 0.5f));

		// 边界点序列：curbRight[0], curbLeft[0], curbRight[1], curbLeft[1], ...按approach.angle
		// (已经在RoadJunction::Build里排好序)——(right_i,left_i)是road i自己的"开口"宽度，
		// (left_i,right_{i+1})是road i与road i+1之间的"桥接"边，两类边交替围成路口多边形。
		TArray<int32> boundaryIdx;
		for (const RoadJunctionApproach& ap : approaches) {
			float curbWorldZ = ap.curbZ * ROAD_WORLD_SCALE + ROADNET_HEIGHT_EPSILON;

			int32 rightIdx = vertices.Num();
			vertices.Add(FVector(ap.curbRight.first * ROAD_WORLD_SCALE, ap.curbRight.second * ROAD_WORLD_SCALE, curbWorldZ));
			uvs.Add(FVector2D(0.f, 0.f));
			boundaryIdx.Add(rightIdx);

			int32 leftIdx = vertices.Num();
			vertices.Add(FVector(ap.curbLeft.first * ROAD_WORLD_SCALE, ap.curbLeft.second * ROAD_WORLD_SCALE, curbWorldZ));
			uvs.Add(FVector2D(1.f, 1.f));
			boundaryIdx.Add(leftIdx);
		}

		// approaches按angle升序(标准数学逆时针)排列，boundaryIdx因此也是逆时针序——扇形三角化
		// 要反过来传(centerIdx,b,a)才是UE要的顺时针/正面朝上环绕，见BuildOpeningMeshes同一条注释。
		int32 m = boundaryIdx.Num();
		for (int32 i = 0; i < m; i++) {
			int32 a = boundaryIdx[i];
			int32 b = boundaryIdx[(i + 1) % m];
			triangles.Add(centerIdx); triangles.Add(b); triangles.Add(a);
		}
	}

	if (triangles.Num() == 0) return;
	junctionMesh->CreateMeshSection(0, vertices, triangles,
		TArray<FVector>(), uvs, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
	if (junctionMaterial) junctionMesh->SetMaterial(0, junctionMaterial);
}

