#include "Framework/ForeverRoadnetFrameworkComponent.h"

#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include "map/map.h"
#include "map/geometry.h"

#define ROAD_WORLD_SCALE 1000.f
// 开口cube/路口mesh都铺在Z=0(地图单位)，和这一片"plain"地形的高度基本重合，会跟地形网格
// 发生共面z-fighting(和ForeverTerrainFrameworkComponent.cpp的HEIGHT_EPSILON是同一个问题)。
// 2mm(世界单位)第一次PIE验证时实测不够(仍然被地形盖住/看不见)，改成5cm验证有效后，
// 按用户要求收到1cm——地形本身按element为单位起伏，1cm相对10m一个格子的尺度依然可以
// 忽略不计，足够压过地形高度采样的误差范围又不会太明显地"浮空"。
#define ROADNET_HEIGHT_EPSILON 10.f

using namespace std;

namespace {
	float SumLanes(const vector<float>& lanes) {
		float sum = 0.f;
		for (float w : lanes) sum += w;
		return sum;
	}
}

UForeverRoadnetFrameworkComponent::UForeverRoadnetFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> roadPlainFinder(
		TEXT("/Game/Asset/Materials/RoadPlain.RoadPlain"));
	if (roadPlainFinder.Succeeded()) {
		roadPlainBaseMaterial = roadPlainFinder.Object;
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

	// 临时演示先跑一次(要在下面BuildRoadInstances/BuildOpeningMeshes之前，
	// 这样它新增的开口能被正确画出来，见ForeverRoadnetFrameworkComponent.md)。
	SpawnAccessNodeDemo();

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
		BuildRoadInstances(road, ts, te);
		BuildOpeningMeshes(road, openingVertices, openingTriangles, openingUvs);
	}
	if (openingTriangles.Num() > 0) {
		openingMesh->CreateMeshSection(0, openingVertices, openingTriangles,
			TArray<FVector>(), openingUvs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		if (openingMaterial) openingMesh->SetMaterial(0, openingMaterial);
	}

	BuildJunctionMeshes();
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

void UForeverRoadnetFrameworkComponent::BuildRoadInstances(Road* road, float trimStart, float trimEnd) {
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
		if (nLow > nHigh || nLow <= 0) return;
		int n = FMath::Clamp(FMath::RoundToInt(rangeLen / unit), nLow, nHigh);
		if (n <= 0) return;

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
	float totalWidth =
		SumLanes(road->GetVehicleLanes(0)) + SumLanes(road->GetVehicleLanes(1)) +
		SumLanes(road->GetParkingLanes(0)) + SumLanes(road->GetParkingLanes(1)) +
		SumLanes(road->GetPedestrianLanes(0)) + SumLanes(road->GetPedestrianLanes(1));
	if (totalWidth <= 0.f) totalWidth = 1.f;

	for (const RoadOpening& op : openings) {
		Node center = road->GetPoint(op.t);
		float dx, dy, dz;
		road->GetTangent(op.t, dx, dy, dz);
		float len = FMath::Sqrt(dx * dx + dy * dy);
		if (len < 1e-6f) len = 1.f;
		float fwdX = dx / len, fwdY = dy / len;
		float perpX = fwdY, perpY = -fwdX;

		float halfLen = op.width * 0.5f;
		float halfWidth = totalWidth * 0.5f;
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

		int32 centerIdx = vertices.Num();
		vertices.Add(FVector(node->GetX() * ROAD_WORLD_SCALE, node->GetY() * ROAD_WORLD_SCALE, ROADNET_HEIGHT_EPSILON));
		uvs.Add(FVector2D(0.5f, 0.5f));

		// 边界点序列：curbRight[0], curbLeft[0], curbRight[1], curbLeft[1], ...按approach.angle
		// (已经在RoadJunction::Build里排好序)——(right_i,left_i)是road i自己的"开口"宽度，
		// (left_i,right_{i+1})是road i与road i+1之间的"桥接"边，两类边交替围成路口多边形。
		TArray<int32> boundaryIdx;
		for (const RoadJunctionApproach& ap : approaches) {
			int32 rightIdx = vertices.Num();
			vertices.Add(FVector(ap.curbRight.first * ROAD_WORLD_SCALE, ap.curbRight.second * ROAD_WORLD_SCALE, ROADNET_HEIGHT_EPSILON));
			uvs.Add(FVector2D(0.f, 0.f));
			boundaryIdx.Add(rightIdx);

			int32 leftIdx = vertices.Num();
			vertices.Add(FVector(ap.curbLeft.first * ROAD_WORLD_SCALE, ap.curbLeft.second * ROAD_WORLD_SCALE, ROADNET_HEIGHT_EPSILON));
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
		TArray<FVector>(), uvs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	if (junctionMaterial) junctionMesh->SetMaterial(0, junctionMaterial);
}

void UForeverRoadnetFrameworkComponent::SpawnAccessNodeDemo() {
	// [临时验证，Building/Zone迁移后可删除] 取第一个有边界Road的lot，在它绑定的其中一条路上
	// 演示一次AddRoadAccessNode，让"车道分裂/开口cube"这套目前还没有真正调用方的逻辑也有
	// 验证途径（效果目前只能通过Map::GetVehicleNavGraph()/GetPedestrianNavGraph()查询或
	// Output Log确认，导航图可视化已按要求移除，见ForeverRoadnetFrameworkComponent.md）。
	if (!map) return;

	for (const auto& [lot, boundary] : map->GetLots()) {
		for (const auto& [dir, road] : boundary) {
			if (!road) continue;

			Node* accessNode = map->AddRoadAccessNode(road->GetName(), 0.5f, true, true, 0.6f);
			if (accessNode) {
				UE_LOG(LogTemp, Log, TEXT("UForeverRoadnetFrameworkComponent: [临时验证] demo access node on road '%s' at map(%.2f,%.2f)。"),
					UTF8_TO_TCHAR(road->GetName().c_str()), accessNode->GetX(), accessNode->GetY());
			}
			return;
		}
	}
}
