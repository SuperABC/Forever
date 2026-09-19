#include "building_shop.h"
#include "room_shop.h"

#include "common/utility.h"

#include <cmath>

using namespace std;

int ShopBuilding::count = 0;

ShopBuilding::ShopBuilding() {
	lastName = string(GetType()) + std::to_string(count++);
}

void ShopBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// direction对显式占位是真实方向，对权重CDF/FillRemainder落地传进来是-1——照抄
	// ResidenceBuilding同款兜底：先从boundaryRoads里随机挑一个有真实边界路的方向，保证这栋
	// building最终有确定方向可用(行人/车辆导航的"outside"端点要靠它找到该连去哪条路)。
	if (direction < 0) {
		std::vector<int> candidates;
		for (auto& [dir, road] : boundaryRoads) {
			if (road) candidates.push_back(dir);
		}
		if (!candidates.empty()) {
			direction = candidates[GetRandom(static_cast<int>(candidates.size()))];
		}
	}

	// 照抄老工程ShopBuilding::PlaceConstruction的占地比例公式(building_basic.cpp:315-328)。
	float scaleX = 0.6f;
	float scaleY = 0.6f;
	if (quad.GetSizeX() < 6.f) scaleX = (quad.GetSizeX() - 2.4f) / quad.GetSizeX();
	if (quad.GetSizeY() < 6.f) scaleY = (quad.GetSizeY() - 2.4f) / quad.GetSizeY();
	footprint = BuildingFootprintSpec{ 0.5f, 0.5f, scaleX, scaleY };

	// 老工程PlaceConstruction直接用这个偏好方向覆盖direction、不检查边界路是否存在——这次和
	// ResidenceBuilding一样，只有偏好方向在boundaryRoads里确实有非空条目时才采用，否则保留
	// 上面已经兜底好的direction，避免行人/车辆导航outside端点查不到路。
	int preferredDirection = (quad.GetSizeX() > quad.GetSizeY())
		? (FACE_WEST + GetRandom(2))
		: (FACE_NORTH + GetRandom(2));
	auto roadIt = boundaryRoads.find(preferredDirection);
	if (roadIt != boundaryRoads.end() && roadIt->second) {
		direction = preferredDirection;
	}

	constexpr const char* kComponent = "component_shop";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.wallMaterial = "/Game/Asset/Materials/White.White"; // 照抄老工程wallTexture

	// layers=2是老工程ShopBuilding::LayoutBuilding的固定值(不像Residence那样按acreage随机
	// 分档)，完整照抄，见building_shop.md。
	layers = 2;

	if (quad.GetAcreage() < 6000.f) {
		basements = 0;
		floorHeights.assign(layers, 0.4f);

		AssignFloor(0, "preset_lobby_linear_fg+", direction, assets);
		AssignRoom(0, 0, ShopRoom::GetId(), kComponent, kComponentId);
		ArrangeRow(0, 0, WarehouseRoom::GetId(), 200.f, kComponent, kComponentId);
		ArrangeRow(0, 1, WarehouseRoom::GetId(), 200.f, kComponent, kComponentId);
		// layers==2时这个循环体是no-op(1 < layers-1 == 1 < 1为false)，照抄老工程原始写法
		// 保留，不做"简化成没有循环"的改写，行为和老工程逐行一致。
		for (int i = 1; i < layers - 1; i++) {
			AssignFloor(i, "preset_lobby_linear_fu+-", direction, assets);
			AssignRoom(i, 0, WarehouseRoom::GetId(), kComponent, kComponentId);
			ArrangeRow(i, 0, WarehouseRoom::GetId(), 200.f, kComponent, kComponentId);
			ArrangeRow(i, 1, WarehouseRoom::GetId(), 200.f, kComponent, kComponentId);
		}
		AssignFloor(layers - 1, "preset_lobby_linear_fu-", direction, assets);
		AssignRoom(layers - 1, 0, WarehouseRoom::GetId(), kComponent, kComponentId);
		ArrangeRow(layers - 1, 0, WarehouseRoom::GetId(), 200.f, kComponent, kComponentId);
		ArrangeRow(layers - 1, 1, WarehouseRoom::GetId(), 200.f, kComponent, kComponentId);
	} else {
		basements = 1;
		floorHeights.assign(basements + layers, 0.4f);

		if (GetRandom(2)) {
			AssignFloor(-1, "preset_circle_double_pg+", direction, assets);
			AssignRoom(-1, 0, ParkingRoom::GetId(), kComponent, kComponentId);
		} else {
			AssignFloor(-1, "preset_circle_double_bg+", direction, assets);
			AssignRoom(-1, 0, WarehouseRoom::GetId(), kComponent, kComponentId);
			AssignRoom(-1, 1, WarehouseRoom::GetId(), kComponent, kComponentId);
			AssignRoom(-1, 2, WarehouseRoom::GetId(), kComponent, kComponentId);
			AssignRoom(-1, 3, WarehouseRoom::GetId(), kComponent, kComponentId);
			AssignRoom(-1, 4, WarehouseRoom::GetId(), kComponent, kComponentId);
			ArrangeRow(-1, 0, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 1, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 2, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 3, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 4, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 5, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 6, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
		}

		AssignFloor(0, "preset_circle_double_fg+-", direction, assets);
		AssignRoom(0, 0, ShopRoom::GetId(), kComponent, kComponentId);
		AssignRoom(0, 1, ShopRoom::GetId(), kComponent, kComponentId);
		AssignRoom(0, 2, WarehouseRoom::GetId(), kComponent, kComponentId);
		AssignRoom(0, 3, WarehouseRoom::GetId(), kComponent, kComponentId);
		AssignRoom(0, 4, WarehouseRoom::GetId(), kComponent, kComponentId);
		AssignRoom(0, 5, WarehouseRoom::GetId(), kComponent, kComponentId);
		ArrangeRow(0, 0, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
		ArrangeRow(0, 1, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
		ArrangeRow(0, 2, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
		ArrangeRow(0, 3, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
		ArrangeRow(0, 4, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
		ArrangeRow(0, 5, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);

		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_circle_double_fu+-", direction, assets);
			AssignRoom(i, 0, WarehouseRoom::GetId(), kComponent, kComponentId);
			AssignRoom(i, 1, WarehouseRoom::GetId(), kComponent, kComponentId);
			AssignRoom(i, 2, WarehouseRoom::GetId(), kComponent, kComponentId);
			AssignRoom(i, 3, WarehouseRoom::GetId(), kComponent, kComponentId);
			AssignRoom(i, 4, WarehouseRoom::GetId(), kComponent, kComponentId);
			ArrangeRow(i, 0, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
			ArrangeRow(i, 1, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
			ArrangeRow(i, 2, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
			ArrangeRow(i, 3, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
			ArrangeRow(i, 4, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
			ArrangeRow(i, 5, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
			ArrangeRow(i, 6, WarehouseRoom::GetId(), 100.f, kComponent, kComponentId);
		}
	}
}

void ShopBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

// 照抄老工程ShopBuilding::RandomAcreage/minAcreage/maxAcreage原始数值。
float ShopBuilding::RandomAcreage() {
	return 4000.f * powf(1.f + GetRandom(1000) / 1000.f * 1.f, 2);
}

float ShopBuilding::GetAcreageMin() {
	return 4000.f;
}

float ShopBuilding::GetAcreageMax() {
	return 16000.f;
}

float ShopBuilding::GetPower(AREA_TYPE area) {
	// 照抄老工程ShopBuilding::GetPowers()：只在商业分区(高/中/低密度)上有权重。
	if (area == AREA_COMMERCIAL_HIGH || area == AREA_COMMERCIAL_MIDDLE || area == AREA_COMMERCIAL_LOW) {
		return 1.f;
	}
	return 0.f;
}
