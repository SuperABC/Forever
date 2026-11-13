#include "building_industry_middle.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>


using namespace std;

namespace {
	constexpr const char* kComponent = "component_residence";
	constexpr int kComponentId = 0;
}

int IndustryMiddleBuilding::count = 0;

IndustryMiddleBuilding::IndustryMiddleBuilding() : id(count++) {
}

const char* IndustryMiddleBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void IndustryMiddleBuilding::Layout(int& direction, const Quad& quad,
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

	// 同ResidenceBuilding：偏好方向在boundaryRoads里有路才采用。
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

	footprint = BuildingFootprintSpec{ 0.5f, 0.5f, 0.6f, 0.6f };

	constexpr const char* kComponent = "component_manufacture";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.wallMaterial = "/Game/Asset/Building/Material/White.White";

	// 地上固定1层，显式赋值不依赖BuildingMod基类默认值。
	layers = 1;
	floorHeights.assign(basements + layers, 0.6f);

	if (basements > 0) {
		AssignFloor(-1, "preset_common_single_pg+", direction, assets);
		AssignRoom(-1, 0, "room_parking", kComponent, kComponentId);
		AssignFloor(0, "preset_common_single_fg-", direction, assets);
	} else {
		AssignFloor(0, "preset_common_single_fg", direction, assets);
	}
	// 三种工坊房间等概率随机选一种：小麦/牛肉/汉堡工厂。
	static const char* const kFactoryRooms[] = { "room_farm", "room_ranch", "room_food_factory" };
	AssignRoom(0, 0, kFactoryRooms[GetRandom(3)], kComponent, kComponentId);
}

void IndustryMiddleBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

// 公式基数2000和GetAcreageMin的4000不一致(RandomAcreage可能小于Min)，是既有数值。
float IndustryMiddleBuilding::RandomAcreage() {
	return 2000.f * powf(1.f + GetRandom(1000) / 1000.f * 1.f, 2);
}

float IndustryMiddleBuilding::GetAcreageMin() {
	return 4000.f;
}

float IndustryMiddleBuilding::GetAcreageMax() {
	return 16000.f;
}

float IndustryMiddleBuilding::GetPower(AREA_TYPE area) {
	// 只在工厂中密度分区上有权重，和其余类型的分区互斥，见building_industry_middle.md。
	if (area == AREA_INDUSTRY_MIDDLE) {
		return 1.f;
	}
	return 0.f;
}
