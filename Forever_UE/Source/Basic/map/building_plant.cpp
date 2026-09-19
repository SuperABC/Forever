#include "building_plant.h"
#include "room_plant.h"
#include "room_shop.h" // ParkingRoom定义在这里(Shop/Factory共用，见room_shop.h顶部注释)

#include "common/utility.h"

#include <cmath>

using namespace std;

int FactoryBuilding::count = 0;

FactoryBuilding::FactoryBuilding() {
	lastName = string(GetType()) + std::to_string(count++);
}

void FactoryBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	if (direction < 0) {
		std::vector<int> candidates;
		for (auto& [dir, road] : boundaryRoads) {
			if (road) candidates.push_back(dir);
		}
		if (!candidates.empty()) {
			direction = candidates[GetRandom(static_cast<int>(candidates.size()))];
		}
	}

	// 照抄老工程FactoryBuilding::LayoutBuilding的方向/地下室判定(building_basic.cpp:464-476)。
	// 老工程这里用一个局部变量`direction`覆盖同名成员、从未真正回写方向(疑似遗留bug)——这次
	// 新API的direction就是唯一的输出，所以这个偏好方向要真正生效，同ResidenceBuilding/
	// ShopBuilding一样，只有boundaryRoads里确实有这个方向的路时才采用。
	int preferredDirection;
	if (quad.GetSizeX() > quad.GetSizeY()) {
		preferredDirection = FACE_NORTH + GetRandom(2);
		if (quad.GetSizeX() > 4.f) basements = 1;
	} else {
		preferredDirection = FACE_WEST + GetRandom(2);
		if (quad.GetSizeY() > 4.f) basements = 1;
	}
	auto roadIt = boundaryRoads.find(preferredDirection);
	if (roadIt != boundaryRoads.end() && roadIt->second) {
		direction = preferredDirection;
	}

	// 照抄老工程PlaceConstruction固定0.6x0.6占地比例(building_basic.cpp:456-458)。
	footprint = BuildingFootprintSpec{ 0.5f, 0.5f, 0.6f, 0.6f };

	constexpr const char* kComponent = "component_factory";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.wallMaterial = "/Game/Asset/Materials/White.White"; // 照抄老工程wallTexture

	// 工厂地上固定1层楼(应用户要求显式写出，不再依赖BuildingMod基类默认值1——老工程
	// FactoryBuilding::LayoutBuilding没有显式设layers，靠基类默认值凑出同样的1层，这次
	// 显式赋值，行为不变但不再依赖那个隐式默认值)。basements是否有地下室由上面quad尺寸
	// 判定，和地上层数无关。
	layers = 1;
	floorHeights.assign(basements + layers, 0.6f);

	if (basements > 0) {
		AssignFloor(-1, "preset_single_room_pg+", direction, assets);
		AssignRoom(-1, 0, ParkingRoom::GetId(), kComponent, kComponentId);
		AssignFloor(0, "preset_single_room_fg-", direction, assets);
	} else {
		AssignFloor(0, "preset_single_room_fg", direction, assets);
	}
	AssignRoom(0, 0, FactoryRoom::GetId(), kComponent, kComponentId);
}

void FactoryBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

// 照抄老工程FactoryBuilding::RandomAcreage/minAcreage/maxAcreage原始数值——公式基数2000
// 和min/max常量4000/16000不一致是老工程本来的数值，照抄不做"修正"。
float FactoryBuilding::RandomAcreage() {
	return 2000.f * powf(1.f + GetRandom(1000) / 1000.f * 1.f, 2);
}

float FactoryBuilding::GetAcreageMin() {
	return 4000.f;
}

float FactoryBuilding::GetAcreageMax() {
	return 16000.f;
}

float FactoryBuilding::GetPower(AREA_TYPE area) {
	// 照抄老工程FactoryBuilding::GetPowers()：只在工业分区(高/中/低密度)上有权重。
	if (area == AREA_INDUSTRIAL_HIGH || area == AREA_INDUSTRIAL_MIDDLE || area == AREA_INDUSTRIAL_LOW) {
		return 1.f;
	}
	return 0.f;
}
