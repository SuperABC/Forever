#include "Framework/ForeverZoneFrameworkComponent.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverStoryFrameworkComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

#include "map/door.h"
#include "map/map.h"
#include "map/zone.h"
#include "map/building.h"
#include "map/general_geometry.h"
#include "story/event.h"

#define ZONE_WORLD_SCALE 1000.f
// 和ForeverRoadnetFrameworkComponent.cpp的ROADNET_HEIGHT_EPSILON同一类问题：Zone没有真实
// 高度数据，围墙实例统一贴地摆在这个高度，避开和地形网格共面z-fighting。
#define ZONE_HEIGHT_EPSILON 10.f
// 园区地面(园区quad扣掉内部建筑quad后的空隙)的高度(cm)：和建筑gap(1楼地面，顶面20cm)同高，BuildingElement.cpp的gap条带顶面。
#define ZONE_GROUND_TOP 20.f

using namespace std;

namespace {
	// building整栋楼的Z范围(世界单位)——和ForeverBuildingFrameworkComponent.cpp的
	// ComputeFullZRange是同一个公式(不同翻译单元的匿名namespace不能跨文件共用，这里按
	// 同样的grade-相对约定重算一遍，用ZONE_HEIGHT_EPSILON这个和BUILDING_HEIGHT_EPSILON
	// 数值相同的本文件专属宏)：最深地下室的底到最高楼层的顶。
	void ComputeBuildingFullZRange(const Building& building, float& outZBottom, float& outZTop) {
		const vector<float>& heights = building.GetFloorHeights();
		int basements = building.GetBasementCount();
		float totalBasementDepth = 0.f;
		for (int i = 0; i < basements && i < static_cast<int>(heights.size()); i++) totalBasementDepth += heights[i];
		float totalAboveHeight = 0.f;
		for (int i = basements; i < static_cast<int>(heights.size()); i++) totalAboveHeight += heights[i];
		outZBottom = ZONE_HEIGHT_EPSILON - totalBasementDepth * ZONE_WORLD_SCALE;
		outZTop = ZONE_HEIGHT_EPSILON + totalAboveHeight * ZONE_WORLD_SCALE;
	}
}

UForeverZoneFrameworkComponent::UForeverZoneFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = false;
}

void UForeverZoneFrameworkComponent::GenerateZones(Map* inMap) {
	map = inMap;
	if (!map) return;

	AActor* owner = GetOwner();
	if (!owner) return;

	// 园区内部建筑改由ForeverBuildingFrameworkComponent统一渲染(map->GetBuildings()天然包含
	// 它们，和顶层building走同一套楼体footprint/楼层/LOD逻辑)；园区大门由ForeverDoorFrameworkComponent按距离流式加载。
	for (auto& [name, zone] : map->GetZones()) {
		if (!zone) continue;
		for (const ZoneWallSpec& wall : zone->GetWalls()) {
			BuildWallSegment(wall, *zone);
		}
		// Zone数量级不大(远小于Room)，碰撞盒直接挂在这个组件的单例owner上，不需要per-zone的Actor。
		BuildCollisionBox(zone);
		BuildDecorations(*zone);
		BuildGround(*zone);
	}
}

void UForeverZoneFrameworkComponent::BuildGround(const Zone& zone) {
	const string& materialPath = zone.GetGapMaterialPath();
	if (materialPath.empty()) return;
	AActor* owner = GetOwner();
	if (!owner) return;

	UMaterialInterface* material = LoadObject<UMaterialInterface>(nullptr, UTF8_TO_TCHAR(materialPath.c_str()));
	if (!material) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverZoneFrameworkComponent::BuildGround: failed to load material %s"),
			UTF8_TO_TCHAR(materialPath.c_str()));
		return;
	}

	// 全部在园区局部系里算(原点在园区中心、轴向与园区一致)：园区quad是轴对齐矩形，
	// 要扣掉的内部建筑quad是带相对旋转的矩形。
	const float halfX = zone.GetSizeX() * 0.5f, halfY = zone.GetSizeY() * 0.5f;
	const float zoneRot = zone.GetRotation();
	const float cosZ = FMath::Cos(zoneRot), sinZ = FMath::Sin(zoneRot);
	auto worldToLocal = [&](float wx, float wy) -> FVector2D {
		float dx = wx - zone.GetPosX(), dy = wy - zone.GetPosY();
		return FVector2D(dx * cosZ + dy * sinZ, -dx * sinZ + dy * cosZ);
	};
	// 以局部点center为中心、长轴方向角angle、尺寸(sx,sy)的矩形四角。
	auto rectCorners = [](FVector2D center, float angle, float sx, float sy) {
		TArray<FVector2D> c;
		float cs = FMath::Cos(angle), sn = FMath::Sin(angle);
		const float sgn[4][2] = { {-1, -1}, {1, -1}, {1, 1}, {-1, 1} };
		for (auto& s : sgn) {
			float lx = s[0] * sx * 0.5f, ly = s[1] * sy * 0.5f;
			c.Add(FVector2D(center.X + lx * cs - ly * sn, center.Y + lx * sn + ly * cs));
		}
		return c;
	};

	TArray<TArray<FVector2D>> obstacles;
	for (const Building* building : zone.GetInternalBuildings()) {
		if (!building) continue;
		obstacles.Add(rectCorners(worldToLocal(building->GetPosX(), building->GetPosY()),
			building->GetRotation() - zoneRot, building->GetSizeX(), building->GetSizeY()));
	}

	// 凸多边形在水平线y上的x区间；线在多边形y范围外返回false。
	auto intervalAt = [](const TArray<FVector2D>& poly, float y, float& outMin, float& outMax) {
		bool hit = false;
		outMin = TNumericLimits<float>::Max();
		outMax = TNumericLimits<float>::Lowest();
		for (int i = 0; i < poly.Num(); i++) {
			const FVector2D& a = poly[i];
			const FVector2D& b = poly[(i + 1) % poly.Num()];
			if (y < FMath::Min(a.Y, b.Y) - 1e-4f || y > FMath::Max(a.Y, b.Y) + 1e-4f) continue;
			hit = true;
			if (FMath::Abs(b.Y - a.Y) < 1e-6f) {
				outMin = FMath::Min(outMin, FMath::Min(a.X, b.X));
				outMax = FMath::Max(outMax, FMath::Max(a.X, b.X));
				continue;
			}
			float x = a.X + (b.X - a.X) * (y - a.Y) / (b.Y - a.Y);
			outMin = FMath::Min(outMin, x);
			outMax = FMath::Max(outMax, x);
		}
		return hit;
	};

	// 水平扫描：断点取所有障碍角点的y，再补上不同障碍边之间交点的y(带内障碍的左右边不再互相穿插)。
	TArray<float> ys = { -halfY, halfY };
	for (const auto& poly : obstacles) {
		for (const FVector2D& p : poly) ys.Add(FMath::Clamp(p.Y, -halfY, halfY));
	}
	for (int i = 0; i < obstacles.Num(); i++) {
		for (int j = i + 1; j < obstacles.Num(); j++) {
			for (int ei = 0; ei < 4; ei++) {
				for (int ej = 0; ej < 4; ej++) {
					FVector2D p = obstacles[i][ei], r = obstacles[i][(ei + 1) % 4] - p;
					FVector2D q = obstacles[j][ej], s = obstacles[j][(ej + 1) % 4] - q;
					float denom = r.X * s.Y - r.Y * s.X;
					if (FMath::Abs(denom) < 1e-8f) continue;
					float u = ((q.X - p.X) * s.Y - (q.Y - p.Y) * s.X) / denom;
					float v = ((q.X - p.X) * r.Y - (q.Y - p.Y) * r.X) / denom;
					if (u < 0.f || u > 1.f || v < 0.f || v > 1.f) continue;
					ys.Add(FMath::Clamp(p.Y + r.Y * u, -halfY, halfY));
				}
			}
		}
	}
	ys.Sort();

	TArray<FVector> vertices;
	TArray<int32> triangles;
	const float z = ZONE_GROUND_TOP;
	auto addQuad = [&](FVector2D a0, FVector2D b0, FVector2D b1, FVector2D a1) {
		int32 base = vertices.Num();
		const FVector2D corners[4] = { a0, b0, b1, a1 };
		for (const FVector2D& p : corners) {
			// 局部 -> 世界(世界单位)
			float wx = zone.GetPosX() * ZONE_WORLD_SCALE + (p.X * cosZ - p.Y * sinZ) * ZONE_WORLD_SCALE;
			float wy = zone.GetPosY() * ZONE_WORLD_SCALE + (p.X * sinZ + p.Y * cosZ) * ZONE_WORLD_SCALE;
			vertices.Add(FVector(wx, wy, z));
		}
		// 正反两种绕序都加：薄片只用来铺材质，不依赖单面剔除方向(两面各只有一面可见，不会互相z-fighting)。
		const int32 order[] = { 0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2 };
		for (int32 index : order) triangles.Add(base + index);
	};

	struct FSpan { float l0, r0, l1, r1, lm; };
	for (int k = 0; k + 1 < ys.Num(); k++) {
		float y0 = ys[k], y1 = ys[k + 1];
		if (y1 - y0 < 1e-4f) continue;
		float ym = (y0 + y1) * 0.5f;
		TArray<FSpan> spans;
		for (const auto& poly : obstacles) {
			float lm, rm, l0, r0, l1, r1;
			if (!intervalAt(poly, ym, lm, rm)) continue;
			intervalAt(poly, y0, l0, r0);
			intervalAt(poly, y1, l1, r1);
			spans.Add({ FMath::Clamp(l0, -halfX, halfX), FMath::Clamp(r0, -halfX, halfX),
				FMath::Clamp(l1, -halfX, halfX), FMath::Clamp(r1, -halfX, halfX), lm });
		}
		spans.Sort([](const FSpan& a, const FSpan& b) { return a.lm < b.lm; });
		// 从左往右：curR0/curR1是目前已被占住的最右边(带底/带顶两端)，下一个障碍左边与它之间就是空隙梯形。
		float curR0 = -halfX, curR1 = -halfX, curRm = -halfX;
		for (const FSpan& span : spans) {
			if (span.lm > curRm + 1e-5f && (span.l0 - curR0 > 1e-4f || span.l1 - curR1 > 1e-4f)) {
				addQuad(FVector2D(curR0, y0), FVector2D(span.l0, y0), FVector2D(span.l1, y1), FVector2D(curR1, y1));
			}
			float rm = (span.r0 + span.r1) * 0.5f;
			if (rm > curRm) { curRm = rm; curR0 = span.r0; curR1 = span.r1; }
		}
		if (halfX - curR0 > 1e-4f || halfX - curR1 > 1e-4f) {
			addQuad(FVector2D(curR0, y0), FVector2D(halfX, y0), FVector2D(halfX, y1), FVector2D(curR1, y1));
		}
	}
	if (vertices.Num() == 0) return;

	UProceduralMeshComponent* comp = NewObject<UProceduralMeshComponent>(owner, NAME_None, RF_Transient);
	comp->SetupAttachment(owner->GetRootComponent());
	comp->SetMobility(EComponentMobility::Static);
	comp->bUseAsyncCooking = false;
	TArray<FVector> normals;
	normals.Init(FVector::UpVector, vertices.Num());
	TArray<FVector2D> uvs;
	uvs.Reserve(vertices.Num());
	for (const FVector& v : vertices) uvs.Add(FVector2D(v.X / ZONE_WORLD_SCALE, v.Y / ZONE_WORLD_SCALE));
	comp->CreateMeshSection(0, vertices, triangles, normals, uvs, TArray<FColor>(), TArray<FProcMeshTangent>(), true); // 开碰撞：平面三角网格，脚踩在上面
	comp->SetMaterial(0, material);
	comp->SetCollisionProfileName(TEXT("BlockAll"));
	comp->SetGenerateOverlapEvents(false);
	comp->RegisterComponent();
	owner->AddInstanceComponent(comp);
}

void UForeverZoneFrameworkComponent::BuildDecorations(const Zone& zone) {
	AActor* owner = GetOwner();
	if (!owner) return;
	for (const DecorationInstance& decoration : zone.GetDecorations()) {
		UStaticMesh* mesh = LoadObject<UStaticMesh>(nullptr, UTF8_TO_TCHAR(decoration.meshPath.c_str()));
		if (!mesh) {
			UE_LOG(LogTemp, Warning, TEXT("UForeverZoneFrameworkComponent::BuildDecorations: failed to load mesh %s"),
				UTF8_TO_TCHAR(decoration.meshPath.c_str()));
			continue;
		}
		// 和ABuildingElement::BuildDecorations同一套摆法：缩放是资产原尺寸的倍数，Z贴园区地坪。
		UStaticMeshComponent* comp = NewObject<UStaticMeshComponent>(owner, NAME_None, RF_Transient);
		comp->SetStaticMesh(mesh);
		comp->SetupAttachment(owner->GetRootComponent());
		comp->SetMobility(EComponentMobility::Static);
		comp->SetGenerateOverlapEvents(false);
		comp->SetWorldLocation(FVector(decoration.x * ZONE_WORLD_SCALE, decoration.y * ZONE_WORLD_SCALE,
			ZONE_HEIGHT_EPSILON + decoration.z * ZONE_WORLD_SCALE));
		comp->SetWorldRotation(FRotator(decoration.pitch, FMath::RadiansToDegrees(decoration.yaw), decoration.roll));
		comp->SetWorldScale3D(FVector(decoration.scaleX, decoration.scaleY, decoration.scaleZ));
		comp->RegisterComponent();
		owner->AddInstanceComponent(comp);
	}
}

UInstancedStaticMeshComponent* UForeverZoneFrameworkComponent::GetOrCreateWallISM(const string& meshPath) {
	if (meshPath.empty()) return nullptr;
	FString key = UTF8_TO_TCHAR(meshPath.c_str());

	if (auto* found = wallMeshInstances.Find(key)) {
		return *found;
	}

	AActor* owner = GetOwner();
	if (!owner) return nullptr;

	UStaticMesh* mesh = LoadObject<UStaticMesh>(nullptr, *key);
	if (!mesh) return nullptr;

	UInstancedStaticMeshComponent* ism = NewObject<UInstancedStaticMeshComponent>(owner, NAME_None, RF_Transient);
	ism->SetStaticMesh(mesh);
	ism->SetupAttachment(owner->GetRootComponent());
	ism->SetMobility(EComponentMobility::Static); // 与Building一致：Mobility显式Static且在RegisterComponent前
	ism->RegisterComponent();
	owner->AddInstanceComponent(ism);

	wallMeshInstances.Add(key, ism);
	return ism;
}

void UForeverZoneFrameworkComponent::BuildWallSegment(const ZoneWallSpec& wall, const Zone& zone) {
	if (wall.mesh.empty() || wall.unit <= 0.f) return;

	UInstancedStaticMeshComponent* ism = GetOrCreateWallISM(wall.mesh);
	if (!ism) return;

	float halfX = zone.GetSizeX() * 0.5f;
	float halfY = zone.GetSizeY() * 0.5f;
	bool alongY = (wall.direction == FACE_WEST || wall.direction == FACE_EAST);

	// 参考边在局部坐标(原点在zone矩形中心)下的位置：WEST在x=-halfX，EAST在x=halfX，
	// NORTH在y=-halfY，SOUTH在y=halfY。
	float edgeX = (wall.direction == FACE_WEST) ? -halfX : (wall.direction == FACE_EAST ? halfX : 0.f);
	float edgeY = (wall.direction == FACE_NORTH) ? -halfY : (wall.direction == FACE_SOUTH ? halfY : 0.f);

	// 进深方向的单位法线："内"指向zone中心：WEST边的内是+X，EAST边的内是-X，NORTH边的内是
	// +Y，SOUTH边的内是-Y；depthInward=false时取反(往外量)。
	float inwardX = 0.f, inwardY = 0.f;
	if (wall.direction == FACE_WEST) inwardX = 1.f;
	else if (wall.direction == FACE_EAST) inwardX = -1.f;
	else if (wall.direction == FACE_NORTH) inwardY = 1.f;
	else inwardY = -1.f;
	float depthDirX = wall.depthInward ? inwardX : -inwardX;
	float depthDirY = wall.depthInward ? inwardY : -inwardY;

	// 必须偏移整个wall.depth(不是depth*0.5f)：沿边方向的marginStart/marginEnd收缩量也是完整的depth，
	// 只偏移一半的话相邻两条墙的中心线端点对不上，墙角会露出缺口。
	float centerAcrossX = edgeX + depthDirX * wall.depth;
	float centerAcrossY = edgeY + depthDirY * wall.depth;

	// 沿边方向(alongY为true时是Y轴，否则是X轴)按marginStart/marginEnd收缩，得到这段围墙的
	// 起止局部坐标(地图单位)。
	float lenStart = alongY ? (-halfY + wall.marginStart) : (-halfX + wall.marginStart);
	float lenEnd = alongY ? (halfY - wall.marginEnd) : (halfX - wall.marginEnd);
	if (lenEnd <= lenStart) return;
	float rangeLenMapUnits = lenEnd - lenStart;

	float rot = zone.GetRotation();
	float cosR = FMath::Cos(rot), sinR = FMath::Sin(rot);

	// 局部坐标(沿边方向的坐标alongCoord，跨边方向固定在centerAcross)转世界坐标(地图单位换算
	// 成世界单位)，和Map::ZoneLocalToWorld同一套旋转公式(局部原点已经在矩形中心，不需要再
	// 减半尺寸)。
	auto localToWorld = [&](float alongCoord) -> FVector {
		float lx = alongY ? centerAcrossX : alongCoord;
		float ly = alongY ? alongCoord : centerAcrossY;
		float wx = zone.GetPosX() + lx * cosR - ly * sinR;
		float wy = zone.GetPosY() + lx * sinR + ly * cosR;
		return FVector(wx * ZONE_WORLD_SCALE, wy * ZONE_WORLD_SCALE, ZONE_HEIGHT_EPSILON);
		};

	float rangeLenWorld = rangeLenMapUnits * ZONE_WORLD_SCALE;
	float unitWorld = wall.unit * ZONE_WORLD_SCALE;

	// 分段数量算法照抄ForeverRoadnetFrameworkComponent::BuildRoadInstances的tileRange："scaled
	// unit落在[0.8,1.2]*unit区间内"这条约束，取最接近rangeLen/unit的合法整数。
	int nLow = FMath::CeilToInt(rangeLenWorld / (1.2f * unitWorld));
	int nHigh = FMath::FloorToInt(rangeLenWorld / (0.8f * unitWorld));
	int n = (nLow <= nHigh && nLow > 0) ? FMath::Clamp(FMath::RoundToInt(rangeLenWorld / unitWorld), nLow, nHigh) : 0;
	if (n <= 0) return; // TODO：这段长度连一节unit都铺不出来时的退化兜底渲染，这次先跳过。

	float actualSegLen = rangeLenWorld / n;
	float scaleAlong = actualSegLen / unitWorld;

	// 局部"沿边方向"单位向量经zone旋转后的世界方向：局部Y方向(0,1)转出(-sinR,cosR)，
	// 局部X方向(1,0)转出(cosR,sinR)。
	FVector worldFwd = alongY ? FVector(-sinR, cosR, 0.f) : FVector(cosR, sinR, 0.f);

	for (int k = 0; k < n; k++) {
		float a0 = lenStart + rangeLenMapUnits * static_cast<float>(k) / n;
		float a1 = lenStart + rangeLenMapUnits * static_cast<float>(k + 1) / n;
		float aMid = (a0 + a1) * 0.5f;
		FVector mid = localToWorld(aMid);

		FTransform xform(worldFwd.Rotation(), mid, FVector(scaleAlong, 1.f, 1.f));
		ism->AddInstance(xform);
	}
}

void UForeverZoneFrameworkComponent::BuildCollisionBox(Zone* zone) {
	if (!zone) return;
	const vector<Building*>& internalBuildings = zone->GetInternalBuildings();
	if (internalBuildings.empty()) return; // 没有内部building的zone没有意义的"内部"体验，跳过。

	AActor* owner = GetOwner();
	if (!owner) return;

	float zBottom = TNumericLimits<float>::Max();
	float zTop = TNumericLimits<float>::Lowest();
	for (Building* building : internalBuildings) {
		if (!building) continue;
		float bBottom, bTop;
		ComputeBuildingFullZRange(*building, bBottom, bTop);
		zBottom = FMath::Min(zBottom, bBottom);
		zTop = FMath::Max(zTop, bTop);
	}
	if (zBottom >= zTop) return; // 所有内部building指针都是空的极端情况，没有有效范围。

	float hx = zone->GetSizeX() * 0.5f * ZONE_WORLD_SCALE;
	float hy = zone->GetSizeY() * 0.5f * ZONE_WORLD_SCALE;
	float cz = (zBottom + zTop) * 0.5f;
	float hz = (zTop - zBottom) * 0.5f;

	UBoxComponent* box = NewObject<UBoxComponent>(owner, NAME_None, RF_Transient);
	box->SetBoxExtent(FVector(hx, hy, hz));
	box->SetCollisionProfileName(TEXT("Trigger"));
	box->SetupAttachment(owner->GetRootComponent());
	// 常驻碰撞盒不会再移动：mobility+世界坐标都要在RegisterComponent()之前设好(Static组件注册后不能再移动)，
	// 见ForeverBuildingFrameworkComponent.md"性能"一节。
	box->SetMobility(EComponentMobility::Static);
	box->SetWorldLocation(FVector(zone->GetPosX() * ZONE_WORLD_SCALE, zone->GetPosY() * ZONE_WORLD_SCALE, cz));
	box->SetWorldRotation(FRotator(0.f, FMath::RadiansToDegrees(zone->GetRotation()), 0.f));
	box->OnComponentBeginOverlap.AddDynamic(this, &UForeverZoneFrameworkComponent::OnZoneOverlapBegin);
	box->OnComponentEndOverlap.AddDynamic(this, &UForeverZoneFrameworkComponent::OnZoneOverlapEnd);
	box->RegisterComponent();
	owner->AddInstanceComponent(box);

	zoneBoxNames.Add(box, zone->GetName());
}

namespace {
	UForeverStoryFrameworkComponent* FindZoneStoryFramework(AForeverFrameworkActor* framework) {
		return framework ? framework->GetStoryFramework() : nullptr;
	}

	// 进出事件只发给这个园区自己的Script：按名字现查(碰撞盒回调里只烘焙了名字，不持有Zone*)。
	Script* FindZoneScript(AForeverFrameworkActor* framework, const std::string& zoneName) {
		Map* map = framework ? framework->GetMap() : nullptr;
		Zone* zone = map ? map->GetZone(zoneName) : nullptr;
		return zone ? zone->GetScript() : nullptr;
	}
}

AForeverFrameworkActor* UForeverZoneFrameworkComponent::GetFrameworkActor() {
	// 缓存弱引用：Overlap回调频繁触发，不要每次GetActorOfClass全World查找。
	AForeverFrameworkActor* framework = cachedFrameworkActor.Get();
	if (!framework) {
		framework = Cast<AForeverFrameworkActor>(
			UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
		cachedFrameworkActor = framework;
	}
	return framework;
}

void UForeverZoneFrameworkComponent::OnZoneOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn) return;
	const std::string* name = zoneBoxNames.Find(OverlappedComponent);
	UForeverStoryFrameworkComponent* storyFramework = FindZoneStoryFramework(GetFrameworkActor());
	if (!name || !storyFramework) return;
	EnterZoneEvent event(*name);
	storyFramework->BroadcastPlayerLocation(event, "zone", *name, /*entering=*/true, FindZoneScript(GetFrameworkActor(), *name));
}

void UForeverZoneFrameworkComponent::OnZoneOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn) return;
	const std::string* name = zoneBoxNames.Find(OverlappedComponent);
	UForeverStoryFrameworkComponent* storyFramework = FindZoneStoryFramework(GetFrameworkActor());
	if (!name || !storyFramework) return;
	LeaveZoneEvent event(*name);
	storyFramework->BroadcastPlayerLocation(event, "zone", *name, /*entering=*/false, FindZoneScript(GetFrameworkActor(), *name));
}
