#include "building_commerce_low.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>


using namespace std;

namespace {
	constexpr const char* kComponent = "component_residence";
	constexpr int kComponentId = 0;
}

int CommerceLowBuilding::count = 0;

CommerceLowBuilding::CommerceLowBuilding() : id(count++) {
}

const char* CommerceLowBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void CommerceLowBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// direction==-1时的兜底，同ResidenceBuilding::Layout。
	if (direction < 0) {
		std::vector<int> candidates;
		for (auto& [dir, road] : boundaryRoads) {
			if (road) candidates.push_back(dir);
		}
		if (!candidates.empty()) {
			direction = candidates[GetRandom(static_cast<int>(candidates.size()))];
		}
	}

	float scaleX = 0.6f;
	float scaleY = 0.6f;
	if (quad.GetSizeX() < 6.f) scaleX = (quad.GetSizeX() - 2.4f) / quad.GetSizeX();
	if (quad.GetSizeY() < 6.f) scaleY = (quad.GetSizeY() - 2.4f) / quad.GetSizeY();
	footprint = BuildingFootprintSpec{ 0.5f, 0.5f, scaleX, scaleY };

	// 同ResidenceBuilding：偏好方向在boundaryRoads里有非空条目才采用。
	int preferredDirection = (quad.GetSizeX() > quad.GetSizeY())
		? (FACE_WEST + GetRandom(2))
		: (FACE_NORTH + GetRandom(2));
	auto roadIt = boundaryRoads.find(preferredDirection);
	if (roadIt != boundaryRoads.end() && roadIt->second) {
		direction = preferredDirection;
	}

	// 商店大门：骨骼动画门，有名字可以交互("敲门")，照抄vehicle_basic.script的
	// game_start->add_option模式，见Resource/Story/door_shop_entrance.script。
	doorSpecs["entrance"].mesh = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/SKM_door_with_cardreader.SKM_door_with_cardreader";
	doorSpecs["entrance"].animClass = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/ABP_Door.ABP_Door_C";
	doorSpecs["entrance"].name = "商店大门";
	doorSpecs["entrance"].scriptModName = "script_empty";
	doorSpecs["entrance"].milestoneNames = { "door_shop_entrance" };

	constexpr const char* kComponent = "component_shop";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.wallMaterial = "/Game/Asset/Building/Material/White.White";

	// layers固定2层(不像Residence按acreage分档)。
	layers = 2;

	if (quad.GetAcreage() < 6000.f) {
		basements = 0;
		floorHeights.assign(layers, 0.4f);

		AssignFloor(0, "preset_lobby_linear_fg+", direction, assets);
		AssignRoom(0, 0, "room_shop", kComponent, kComponentId);
		ArrangeRow(0, 0, "room_warehouse", 200.f, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_warehouse", 200.f, kComponent, kComponentId);
		// layers==2时这个循环体是no-op(1 < layers-1 == 1 < 1为false)，保留循环结构不简化，
		// layers以后如果改成可变值这段逻辑会自动生效。
		for (int i = 1; i < layers - 1; i++) {
			AssignFloor(i, "preset_lobby_linear_fu+-", direction, assets);
			AssignRoom(i, 0, "room_warehouse", kComponent, kComponentId);
			ArrangeRow(i, 0, "room_warehouse", 200.f, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_warehouse", 200.f, kComponent, kComponentId);
		}
		AssignFloor(layers - 1, "preset_lobby_linear_fu-", direction, assets);
		AssignRoom(layers - 1, 0, "room_warehouse", kComponent, kComponentId);
		ArrangeRow(layers - 1, 0, "room_warehouse", 200.f, kComponent, kComponentId);
		ArrangeRow(layers - 1, 1, "room_warehouse", 200.f, kComponent, kComponentId);
	} else {
		basements = 1;
		floorHeights.assign(basements + layers, 0.4f);

		if (GetRandom(2)) {
			AssignFloor(-1, "preset_circle_double_pg+", direction, assets);
			AssignRoom(-1, 0, "room_parking", kComponent, kComponentId);
		} else {
			AssignFloor(-1, "preset_circle_double_bg+", direction, assets);
			AssignRoom(-1, 0, "room_warehouse", kComponent, kComponentId);
			AssignRoom(-1, 1, "room_warehouse", kComponent, kComponentId);
			AssignRoom(-1, 2, "room_warehouse", kComponent, kComponentId);
			AssignRoom(-1, 3, "room_warehouse", kComponent, kComponentId);
			AssignRoom(-1, 4, "room_warehouse", kComponent, kComponentId);
			ArrangeRow(-1, 0, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 1, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 2, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 3, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 4, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 5, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(-1, 6, "room_warehouse", 100.f, kComponent, kComponentId);
		}

		AssignFloor(0, "preset_circle_double_fg+-", direction, assets);
		AssignRoom(0, 0, "room_shop", kComponent, kComponentId);
		AssignRoom(0, 1, "room_shop", kComponent, kComponentId);
		AssignRoom(0, 2, "room_warehouse", kComponent, kComponentId);
		AssignRoom(0, 3, "room_warehouse", kComponent, kComponentId);
		AssignRoom(0, 4, "room_warehouse", kComponent, kComponentId);
		AssignRoom(0, 5, "room_warehouse", kComponent, kComponentId);
		ArrangeRow(0, 0, "room_warehouse", 100.f, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_warehouse", 100.f, kComponent, kComponentId);
		ArrangeRow(0, 2, "room_warehouse", 100.f, kComponent, kComponentId);
		ArrangeRow(0, 3, "room_warehouse", 100.f, kComponent, kComponentId);
		ArrangeRow(0, 4, "room_warehouse", 100.f, kComponent, kComponentId);
		ArrangeRow(0, 5, "room_warehouse", 100.f, kComponent, kComponentId);

		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_circle_double_fu+-", direction, assets);
			AssignRoom(i, 0, "room_warehouse", kComponent, kComponentId);
			AssignRoom(i, 1, "room_warehouse", kComponent, kComponentId);
			AssignRoom(i, 2, "room_warehouse", kComponent, kComponentId);
			AssignRoom(i, 3, "room_warehouse", kComponent, kComponentId);
			AssignRoom(i, 4, "room_warehouse", kComponent, kComponentId);
			ArrangeRow(i, 0, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(i, 2, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(i, 3, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(i, 4, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(i, 5, "room_warehouse", 100.f, kComponent, kComponentId);
			ArrangeRow(i, 6, "room_warehouse", 100.f, kComponent, kComponentId);
		}
	}
}

void CommerceLowBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

float CommerceLowBuilding::RandomAcreage() {
	return 4000.f * powf(1.f + GetRandom(1000) / 1000.f * 1.f, 2);
}

float CommerceLowBuilding::GetAcreageMin() {
	return 4000.f;
}

float CommerceLowBuilding::GetAcreageMax() {
	return 16000.f;
}

float CommerceLowBuilding::GetPower(AREA_TYPE area) {
	// 只在商店低密度分区上有权重，和其余类型的分区互斥，见building_commerce_low.md。
	if (area == AREA_COMMERCE_LOW) {
		return 1.f;
	}
	return 0.f;
}
