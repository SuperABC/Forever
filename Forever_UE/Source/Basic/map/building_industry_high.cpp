#include "building_industry_high.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>


using namespace std;

namespace {
	constexpr const char* kComponent = "component_residence";
	constexpr int kComponentId = 0;
}

int IndustryHighBuilding::count = 0;

IndustryHighBuilding::IndustryHighBuilding() : id(count++) {
}

const char* IndustryHighBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void IndustryHighBuilding::Layout(int& direction, const Quad& quad,
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
	int preferredDirection = quad.GetSizeX() > quad.GetSizeY() ? FACE_NORTH + GetRandom(2) : FACE_WEST + GetRandom(2);
	auto roadIt = boundaryRoads.find(preferredDirection);
	if (roadIt != boundaryRoads.end() && roadIt->second) {
		direction = preferredDirection;
	}

	footprint = BuildingFootprintSpec{ 0.5f, 0.5f, 0.6f, 0.6f };
	SetSurfaceMaterial("clay", "/Game/Asset/Building/Material/Clay.Clay");
	// 楼体与建筑范围之间的空隙铺黏土材质。
	gapMaterial = "/Game/Asset/Building/Material/Clay.Clay";

	constexpr const char* kComponent = "component_manufacture";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.wallMaterial = "/Game/Asset/Building/Material/White.White";

	// common_single：地下1层pg+(停车库)、地上1层fg-，显式赋值不依赖BuildingMod基类默认值。
	basements = 1;
	layers = 1;
	floorHeights.assign(basements + layers, 0.6f);

	AssignFloor(-1, "preset_common_single_pg+", direction, assets);
	AssignRoom(-1, 0, "room_parking", kComponent, kComponentId);
	AssignFloor(0, "preset_common_single_fg-", direction, assets);
	// 三种工坊房间等概率随机选一种：小麦/牛肉/汉堡工厂。
	static const char* const kFactoryRooms[] = { "room_farm", "room_ranch", "room_food_factory" };
	AssignRoom(0, 0, kFactoryRooms[GetRandom(3)], kComponent, kComponentId);
}

void IndustryHighBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

float IndustryHighBuilding::RandomAcreage() {
	return 10000.f + 10000.f * GetRandom(1000) / 1000.f; // 10000~20000线性随机
}

float IndustryHighBuilding::GetAcreageMin() {
	return 10000.f;
}

float IndustryHighBuilding::GetAcreageMax() {
	return 20000.f;
}

float IndustryHighBuilding::GetPower(AREA_TYPE area) {
	// 只在工厂高密度分区上有权重，和其余类型的分区互斥，见building_industry_high.md。
	if (area == AREA_INDUSTRY_HIGH) {
		return 1.f;
	}
	return 0.f;
}
