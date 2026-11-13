#include "building_office_low.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>


using namespace std;

namespace {
	constexpr const char* kComponent = "component_residence";
	constexpr int kComponentId = 0;
}

int OfficeLowBuilding::count = 0;

OfficeLowBuilding::OfficeLowBuilding() : id(count++) {
}

const char* OfficeLowBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void OfficeLowBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

float OfficeLowBuilding::RandomAcreage() {
	return 2000.f * powf(1.f + GetRandom(1000) / 1000.f * 2.f, 2);
}

float OfficeLowBuilding::GetAcreageMin() {
	return 2000.f;
}

float OfficeLowBuilding::GetAcreageMax() {
	return 18000.f;
}

void OfficeLowBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// layout照抄住宅(模板/电梯/楼层数分档都一样)，房间换成办公室；不挂住宅的entity_event示例脚本。
	// direction对显式占位是真实方向，权重CDF/FillRemainder落地时是-1：-1时从boundaryRoads随机
	// 挑一个有真实边界路的方向回写，行人/车辆导航的"outside"端点要靠它找到该连的路，见
	// building_mod.h的Layout()注释。
	if (direction < 0) {
		std::vector<int> candidates;
		for (auto& [dir, road] : boundaryRoads) {
			if (road) candidates.push_back(dir);
		}
		if (!candidates.empty()) {
			direction = candidates[GetRandom(static_cast<int>(candidates.size()))];
		}
	}

	footprint = BuildingFootprintSpec{ 0.5f, 0.5f, 0.8f, 0.8f }; // 楼体占地块80%，居中
	basements = 1;
	// 楼层数按地块面积分档。
	if (quad.GetAcreage() < 5000.f) {
		layers = 3 + GetRandom(3);
	} else if (quad.GetAcreage() < 10000.f) {
		layers = 6 + GetRandom(4);
	} else {
		layers = 10 + GetRandom(5);
	}
	floorHeights.clear();
	floorHeights.push_back(0.35f); // 地下室
	// 地上每层高度统一0.4，不循环取不同值。
	for (int i = 0; i < layers; i++) {
		floorHeights.push_back(0.4f);
	}
	// lodMaterial留空 -> 用渲染层默认灰色Pure MID

	// 建筑大门：骨骼动画门(目前所有门共用测试门)。只有corridor的门洞带了"entrance"这个tag才会用到这份
	// spec，见Core/map/door.h的ResolveDoorSpec。
	doorSpecs["entrance"].mesh = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/SKM_door_with_cardreader.SKM_door_with_cardreader";
	doorSpecs["entrance"].animClass = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/ABP_Door.ABP_Door_C";

	constexpr const char* kComponent = "component_office";
	constexpr int kComponentId = 0;

	// 按quad尺寸分档随机选一套模板组合(layout 0-3)+房间槽位密度(size)。只有layout==1/3的模板
	// (preset_lobby_wing_*/preset_nshape_double_*)带电梯井道，AssignElevatorCabin靠它们产出Elevator几何。
	// 偏好方向只有在boundaryRoads里确实有非空条目时才采用，否则保留上面兜底出来的direction
	// (否则outside端点会查不到路)。
	int layout = 0;
	float size = 120.f;
	int preferredDirection = direction;
	if (quad.GetSizeX() <= 3.f || quad.GetSizeY() <= 3.f) {
		layout = 0;
		size = 40.f;
		if (quad.GetSizeX() > 3.f) preferredDirection = FACE_WEST + GetRandom(2);
		if (quad.GetSizeY() > 3.f) preferredDirection = FACE_NORTH + GetRandom(2);
	} else if (quad.GetSizeX() <= 6.f || quad.GetSizeY() <= 6.f) {
		layout = GetRandom(2);
		size = 160.f;
		if (quad.GetSizeX() > 6.f) preferredDirection = layout * 2 + GetRandom(2);
		if (quad.GetSizeY() > 6.f) preferredDirection = 2 - layout * 2 + GetRandom(2);
	} else if (quad.GetSizeX() <= 9.f || quad.GetSizeY() <= 9.f) {
		layout = 2;
		size = 120.f;
		if (quad.GetSizeX() > 9.f) preferredDirection = FACE_WEST + GetRandom(2);
		if (quad.GetSizeY() > 9.f) preferredDirection = FACE_NORTH + GetRandom(2);
	} else {
		layout = 3;
		size = 120.f;
		if (quad.GetSizeX() > quad.GetSizeY()) preferredDirection = FACE_WEST + GetRandom(2);
		else preferredDirection = FACE_NORTH + GetRandom(2);
	}
	auto roadIt = boundaryRoads.find(preferredDirection);
	if (roadIt != boundaryRoads.end() && roadIt->second) {
		direction = preferredDirection;
	}

	if (layout == 0) {
		AssignFloor(-1, "preset_straight_linear_bg+", direction);
		ArrangeRow(-1, 0, "room_office", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_office", size, kComponent, kComponentId);
		AssignFloor(0, "preset_straight_linear_fg+-", direction);
		ArrangeRow(0, 0, "room_office", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_office", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_straight_linear_fu+-", direction);
			ArrangeRow(i, 0, "room_office", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_office", size, kComponent, kComponentId);
		}
	} else if (layout == 1) {
		AssignFloor(-1, "preset_lobby_wing_bg+", direction);
		ArrangeRow(-1, 0, "room_office", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_office", size, kComponent, kComponentId);
		ArrangeRow(-1, 2, "room_office", size, kComponent, kComponentId);
		ArrangeRow(-1, 3, "room_office", size, kComponent, kComponentId);
		AssignFloor(0, "preset_lobby_wing_fg+-", direction);
		ArrangeRow(0, 0, "room_office", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_office", size, kComponent, kComponentId);
		ArrangeRow(0, 2, "room_office", size, kComponent, kComponentId);
		ArrangeRow(0, 3, "room_office", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_lobby_wing_fu+-", direction);
			ArrangeRow(i, 0, "room_office", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_office", size, kComponent, kComponentId);
			ArrangeRow(i, 2, "room_office", size, kComponent, kComponentId);
			ArrangeRow(i, 3, "room_office", size, kComponent, kComponentId);
		}
		AssignElevatorCabin(0, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
		AssignElevatorCabin(1, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	} else if (layout == 2) {
		AssignFloor(-1, "preset_lshape_double_bg+", direction);
		AssignRoom(-1, 0, "room_office", kComponent, kComponentId);
		AssignRoom(-1, 1, "room_office", kComponent, kComponentId);
		ArrangeRow(-1, 0, "room_office", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_office", size, kComponent, kComponentId);
		ArrangeRow(-1, 2, "room_office", size, kComponent, kComponentId);
		ArrangeRow(-1, 3, "room_office", size, kComponent, kComponentId);
		ArrangeRow(-1, 4, "room_office", size, kComponent, kComponentId);
		AssignFloor(0, "preset_lshape_double_fg+-", direction);
		AssignRoom(0, 0, "room_office", kComponent, kComponentId);
		AssignRoom(0, 1, "room_office", kComponent, kComponentId);
		ArrangeRow(0, 0, "room_office", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_office", size, kComponent, kComponentId);
		ArrangeRow(0, 2, "room_office", size, kComponent, kComponentId);
		ArrangeRow(0, 3, "room_office", size, kComponent, kComponentId);
		ArrangeRow(0, 4, "room_office", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_lshape_double_fu+-", direction);
			AssignRoom(i, 0, "room_office", kComponent, kComponentId);
			AssignRoom(i, 1, "room_office", kComponent, kComponentId);
			ArrangeRow(i, 0, "room_office", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_office", size, kComponent, kComponentId);
			ArrangeRow(i, 2, "room_office", size, kComponent, kComponentId);
			ArrangeRow(i, 3, "room_office", size, kComponent, kComponentId);
			ArrangeRow(i, 4, "room_office", size, kComponent, kComponentId);
		}
	} else { // layout == 3
		AssignFloor(-1, "preset_nshape_double_bg+", direction);
		AssignRoom(-1, 0, "room_empty", kComponent, kComponentId); // 占位槽位，不铺真实房间
		AssignRoom(-1, 1, "room_office", kComponent, kComponentId);
		AssignRoom(-1, 2, "room_office", kComponent, kComponentId);
		ArrangeRow(-1, 0, "room_office", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_office", size, kComponent, kComponentId);
		ArrangeRow(-1, 2, "room_office", size, kComponent, kComponentId);
		ArrangeRow(-1, 3, "room_office", size, kComponent, kComponentId);
		ArrangeRow(-1, 4, "room_office", size, kComponent, kComponentId);
		ArrangeRow(-1, 5, "room_office", size, kComponent, kComponentId);
		ArrangeRow(-1, 6, "room_office", size, kComponent, kComponentId);
		ArrangeRow(-1, 7, "room_office", size, kComponent, kComponentId);
		AssignFloor(0, "preset_nshape_double_fg+-", direction);
		AssignRoom(0, 0, "room_empty", kComponent, kComponentId);
		AssignRoom(0, 1, "room_office", kComponent, kComponentId);
		AssignRoom(0, 2, "room_office", kComponent, kComponentId);
		ArrangeRow(0, 0, "room_office", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_office", size, kComponent, kComponentId);
		ArrangeRow(0, 2, "room_office", size, kComponent, kComponentId);
		ArrangeRow(0, 3, "room_office", size, kComponent, kComponentId);
		ArrangeRow(0, 4, "room_office", size, kComponent, kComponentId);
		ArrangeRow(0, 5, "room_office", size, kComponent, kComponentId);
		ArrangeRow(0, 6, "room_office", size, kComponent, kComponentId);
		ArrangeRow(0, 7, "room_office", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_nshape_double_fu+-", direction);
			AssignRoom(i, 0, "room_empty", kComponent, kComponentId);
			AssignRoom(i, 1, "room_office", kComponent, kComponentId);
			AssignRoom(i, 2, "room_office", kComponent, kComponentId);
			ArrangeRow(i, 0, "room_office", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_office", size, kComponent, kComponentId);
			ArrangeRow(i, 2, "room_office", size, kComponent, kComponentId);
			ArrangeRow(i, 3, "room_office", size, kComponent, kComponentId);
			ArrangeRow(i, 4, "room_office", size, kComponent, kComponentId);
			ArrangeRow(i, 5, "room_office", size, kComponent, kComponentId);
			ArrangeRow(i, 6, "room_office", size, kComponent, kComponentId);
			ArrangeRow(i, 7, "room_office", size, kComponent, kComponentId);
		}
		AssignElevatorCabin(0, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
		AssignElevatorCabin(1, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	}
}

float OfficeLowBuilding::GetPower(AREA_TYPE area) {
	// 只在办公楼低密度分区上有权重，和其余类型的分区互斥，见building_office_low.md。
	if (area == AREA_OFFICE_LOW) {
		return 1.f;
	}
	return 0.f;
}
