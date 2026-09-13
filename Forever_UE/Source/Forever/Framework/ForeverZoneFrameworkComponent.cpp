#include "Framework/ForeverZoneFrameworkComponent.h"

#include "Components/InstancedStaticMeshComponent.h"

#include "map/map.h"
#include "map/zone.h"
#include "map/geometry.h"

#define ZONE_WORLD_SCALE 1000.f
// 和ForeverRoadnetFrameworkComponent.cpp的ROADNET_HEIGHT_EPSILON同一类问题：Zone没有真实
// 高度数据，围墙实例统一贴地摆在这个高度，避开和地形网格共面z-fighting。
#define ZONE_HEIGHT_EPSILON 10.f

using namespace std;

UForeverZoneFrameworkComponent::UForeverZoneFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = false;
}

void UForeverZoneFrameworkComponent::GenerateZones(Map* inMap) {
	map = inMap;
	if (!map) return;

	AActor* owner = GetOwner();
	if (!owner) return;

	// 园区内部建筑改由ForeverBuildingFrameworkComponent统一渲染(map->GetBuildings()天然包含
	// 它们，和顶层building走同一套楼体footprint/楼层/LOD逻辑)，这里只处理围墙。
	for (auto& [name, zone] : map->GetZones()) {
		if (!zone) continue;
		for (const ZoneWallSpec& wall : zone->GetWalls()) {
			BuildWallSegment(wall, *zone);
		}
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

	// 这里必须偏移整个wall.depth(不是depth*0.5f)——沿边方向的marginStart/marginEnd收缩量
	// 用的是完整的depth(见下面lenStart/lenEnd)，两个轴描述的是同一个"整体往里缩进depth"，
	// 如果这里只偏移depth的一半，围墙的中心线实际只往里缩了depth/2，跟沿边方向缩进的完整
	// depth对不上：相邻两条墙的中心线端点就会分别停在两个不同的点上，而不是同一个点，
	// 拼出来的墙角自然就会露出缺口(缺口宽度正好是这里少偏移掉的depth/2)。
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
