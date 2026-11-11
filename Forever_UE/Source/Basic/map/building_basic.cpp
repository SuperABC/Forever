#include "building_basic.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>


using namespace std;

namespace {
	// 把楼体压到靠近道路的一侧：深度方向只占bodyDepthSizeRatio、贴着道路那条边，剩下的深处空间
	// 留给StationMod的站台/跑道，避免公共交通连线穿过楼体。bodyDepthCenterRatio按"贴道路=0、
	// 远离道路=1"给，direction->深度轴的映射和station_basic.cpp的NearRoadPoint一致(那边用
	// 单点坐标，这里用区间(centerRatio,sizeRatio)，换算不同所以单独写一份)。
	// bodyFrontageSizeRatio是沿道路方向的占比，不随direction变化。
	BuildingFootprintSpec NearRoadFootprint(int direction, float bodyDepthCenterRatio,
		float bodyDepthSizeRatio, float bodyFrontageSizeRatio) {
		bool depthAlongX = (direction == FACE_WEST || direction == FACE_EAST);
		bool flip = (direction == FACE_EAST || direction == FACE_NORTH);
		float depthCenter = flip ? 1.f - bodyDepthCenterRatio : bodyDepthCenterRatio;

		BuildingFootprintSpec spec;
		if (depthAlongX) {
			spec.centerRatioX = depthCenter; spec.sizeRatioX = bodyDepthSizeRatio;
			spec.centerRatioY = 0.5f; spec.sizeRatioY = bodyFrontageSizeRatio;
		} else {
			spec.centerRatioY = depthCenter; spec.sizeRatioY = bodyDepthSizeRatio;
			spec.centerRatioX = 0.5f; spec.sizeRatioX = bodyFrontageSizeRatio;
		}
		return spec;
	}
}

int ResidenceHighBuilding::count = 0;

ResidenceHighBuilding::ResidenceHighBuilding() : id(count++) {
}

const char* ResidenceHighBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void ResidenceHighBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 挂示例脚本entity_event(进出事件只发给这个实体自己的Script)，见Resource/Story/entity_event.script。
	milestoneNames = { "entity_event" };
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

	// side布局：地上7~12层随机，没有地下室。
	basements = 0;
	layers = 7 + GetRandom(6);
	floorHeights.clear();
	// 每层高度统一0.4。
	for (int i = 0; i < layers; i++) {
		floorHeights.push_back(0.4f);
	}
	// lodMaterial留空 -> 用渲染层默认灰色Pure MID

	constexpr const char* kComponent = "component_residence";
	constexpr int kComponentId = 0;

	// 只用preset_residential_side系列模板(1楼fg+、顶层fu-、中间fu+-)(原wing布局的C++备份见building_basic_wing_backup.txt)。
	// 偏好方向：长边朝向正对的路，
	// 只有boundaryRoads里确实有非空条目时才采用，否则保留上面兜底出来的direction(否则outside端点会查不到路)。
	int preferredDirection = quad.GetSizeX() > quad.GetSizeY() ? FACE_NORTH + GetRandom(2) : FACE_WEST + GetRandom(2);
	auto roadIt = boundaryRoads.find(preferredDirection);
	if (roadIt != boundaryRoads.end() && roadIt->second) {
		direction = preferredDirection;
	}

	// 楼体摆法：朝向轴(进深)上最多3(地图单位)，地块没这么深就占满，更深就取3并居中；
	// 垂直于进深的方向(开间)居中，两侧各留kSideGap的空隙。
	constexpr float kMaxDepth = 3.f;
	constexpr float kSideGap = 0.2f;
	footprint = BuildingFootprintSpec{ 0.5f, 0.5f, 1.f, 1.f };
	if (direction == FACE_WEST || direction == FACE_EAST) {
		footprint.sizeRatioX = min(kMaxDepth, quad.GetSizeX()) / quad.GetSizeX();
		footprint.sizeRatioY = max(0.f, quad.GetSizeY() - 2.f * kSideGap) / quad.GetSizeY();
	} else {
		footprint.sizeRatioY = min(kMaxDepth, quad.GetSizeY()) / quad.GetSizeY();
		footprint.sizeRatioX = max(0.f, quad.GetSizeX() - 2.f * kSideGap) / quad.GetSizeX();
	}

	// 墙面/地面/屋顶标签 -> 材质实例(side与wing两套layout用到的标签)。
	SetSurfaceMaterial("roof", "/Game/Asset/Building/Material/Black.Black");
	SetSurfaceMaterial("brick", "/Game/Asset/Building/Material/Brick.Brick");
	SetSurfaceMaterial("concrete", "/Game/Asset/Building/Material/Concrete.Concrete");
	SetSurfaceMaterial("clay", "/Game/Asset/Building/Material/Clay.Clay");
	SetSurfaceMaterial("pavement", "/Game/Asset/Building/Material/Pavement.Pavement");
	// 楼体与建筑范围之间的空隙铺人行道材质。
	gapMaterial = "/Game/Asset/Building/Material/Pavement.Pavement";

	// 1楼fg+，顶层fu-，中间fu+-；每层8个single，4个电梯井道贯穿全楼。
	FloorAssetSpec assets;
	assets.stairMeshPath = "/Game/Asset/Building/Mesh/Stair_Line.Stair_Line"; // 直跑楼梯，不用默认的Stair_Return
	for (int level = 0; level < layers; level++) {
		const char* templateName = level == 0 ? "preset_residential_side_fg+"
			: level == layers - 1 ? "preset_residential_side_fu-" : "preset_residential_side_fu+-";
		AssignFloor(level, templateName, direction, assets);
		for (int slot = 0; slot < 8; slot++) {
			AssignRoom(level, slot, "room_residence", kComponent, kComponentId);
		}
	}
	for (int elevator = 0; elevator < 4; elevator++) {
		AssignElevatorCabin(elevator, 0, layers - 1, "", "script_empty", { "elevator_basic" });
	}
}

void ResidenceHighBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

float ResidenceHighBuilding::RandomAcreage() {
	return 2000.f * powf(1.f + GetRandom(1000) / 1000.f, 2); // 2000~8000，平方使小地块更常见
}

float ResidenceHighBuilding::GetAcreageMin() {
	return 2000.f;
}

float ResidenceHighBuilding::GetAcreageMax() {
	return 8000.f;
}

float ResidenceHighBuilding::GetPower(AREA_TYPE area) {
	// 只在住宅高密度分区上有权重，和其余类型的分区互斥，见building_basic.md。
	if (area == AREA_TEST) {
		return 1.f;
	}
	if (area == AREA_RESIDENTIAL_HIGH) {
		return 1.f;
	}
	return 0.f;
}

int ResidenceMiddleBuilding::count = 0;

ResidenceMiddleBuilding::ResidenceMiddleBuilding() : id(count++) {
}

const char* ResidenceMiddleBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void ResidenceMiddleBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 挂示例脚本entity_event(进出事件只发给这个实体自己的Script)，见Resource/Story/entity_event.script。
	milestoneNames = { "entity_event" };
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

	constexpr const char* kComponent = "component_residence";
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
		ArrangeRow(-1, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_residence", size, kComponent, kComponentId);
		AssignFloor(0, "preset_straight_linear_fg+-", direction);
		ArrangeRow(0, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_residence", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_straight_linear_fu+-", direction);
			ArrangeRow(i, 0, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_residence", size, kComponent, kComponentId);
		}
	} else if (layout == 1) {
		AssignFloor(-1, "preset_lobby_wing_bg+", direction);
		ArrangeRow(-1, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 3, "room_residence", size, kComponent, kComponentId);
		AssignFloor(0, "preset_lobby_wing_fg+-", direction);
		ArrangeRow(0, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 3, "room_residence", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_lobby_wing_fu+-", direction);
			ArrangeRow(i, 0, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 2, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 3, "room_residence", size, kComponent, kComponentId);
		}
		AssignElevatorCabin(0, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
		AssignElevatorCabin(1, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	} else if (layout == 2) {
		AssignFloor(-1, "preset_lshape_double_bg+", direction);
		AssignRoom(-1, 0, "room_residence", kComponent, kComponentId);
		AssignRoom(-1, 1, "room_residence", kComponent, kComponentId);
		ArrangeRow(-1, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 3, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 4, "room_residence", size, kComponent, kComponentId);
		AssignFloor(0, "preset_lshape_double_fg+-", direction);
		AssignRoom(0, 0, "room_residence", kComponent, kComponentId);
		AssignRoom(0, 1, "room_residence", kComponent, kComponentId);
		ArrangeRow(0, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 3, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 4, "room_residence", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_lshape_double_fu+-", direction);
			AssignRoom(i, 0, "room_residence", kComponent, kComponentId);
			AssignRoom(i, 1, "room_residence", kComponent, kComponentId);
			ArrangeRow(i, 0, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 2, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 3, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 4, "room_residence", size, kComponent, kComponentId);
		}
	} else { // layout == 3
		AssignFloor(-1, "preset_nshape_double_bg+", direction);
		AssignRoom(-1, 0, "room_empty", kComponent, kComponentId); // 占位槽位，不铺真实房间
		AssignRoom(-1, 1, "room_residence", kComponent, kComponentId);
		AssignRoom(-1, 2, "room_residence", kComponent, kComponentId);
		ArrangeRow(-1, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 3, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 4, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 5, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 6, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 7, "room_residence", size, kComponent, kComponentId);
		AssignFloor(0, "preset_nshape_double_fg+-", direction);
		AssignRoom(0, 0, "room_empty", kComponent, kComponentId);
		AssignRoom(0, 1, "room_residence", kComponent, kComponentId);
		AssignRoom(0, 2, "room_residence", kComponent, kComponentId);
		ArrangeRow(0, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 3, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 4, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 5, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 6, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 7, "room_residence", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_nshape_double_fu+-", direction);
			AssignRoom(i, 0, "room_empty", kComponent, kComponentId);
			AssignRoom(i, 1, "room_residence", kComponent, kComponentId);
			AssignRoom(i, 2, "room_residence", kComponent, kComponentId);
			ArrangeRow(i, 0, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 2, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 3, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 4, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 5, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 6, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 7, "room_residence", size, kComponent, kComponentId);
		}
		AssignElevatorCabin(0, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
		AssignElevatorCabin(1, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	}
}

void ResidenceMiddleBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

float ResidenceMiddleBuilding::RandomAcreage() {
	return 2000.f * powf(1.f + GetRandom(1000) / 1000.f * 2.f, 2);
}

float ResidenceMiddleBuilding::GetAcreageMin() {
	return 2000.f;
}

float ResidenceMiddleBuilding::GetAcreageMax() {
	return 18000.f;
}

float ResidenceMiddleBuilding::GetPower(AREA_TYPE area) {
	// 只在住宅中密度分区上有权重，和其余类型的分区互斥，见building_basic.md。
	if (area == AREA_RESIDENTIAL_MIDDLE) {
		return 1.f;
	}
	return 0.f;
}

int ResidenceLowBuilding::count = 0;

ResidenceLowBuilding::ResidenceLowBuilding() : id(count++) {
}

const char* ResidenceLowBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void ResidenceLowBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 挂示例脚本entity_event(进出事件只发给这个实体自己的Script)，见Resource/Story/entity_event.script。
	milestoneNames = { "entity_event" };
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

	constexpr const char* kComponent = "component_residence";
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
		ArrangeRow(-1, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_residence", size, kComponent, kComponentId);
		AssignFloor(0, "preset_straight_linear_fg+-", direction);
		ArrangeRow(0, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_residence", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_straight_linear_fu+-", direction);
			ArrangeRow(i, 0, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_residence", size, kComponent, kComponentId);
		}
	} else if (layout == 1) {
		AssignFloor(-1, "preset_lobby_wing_bg+", direction);
		ArrangeRow(-1, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 3, "room_residence", size, kComponent, kComponentId);
		AssignFloor(0, "preset_lobby_wing_fg+-", direction);
		ArrangeRow(0, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 3, "room_residence", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_lobby_wing_fu+-", direction);
			ArrangeRow(i, 0, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 2, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 3, "room_residence", size, kComponent, kComponentId);
		}
		AssignElevatorCabin(0, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
		AssignElevatorCabin(1, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	} else if (layout == 2) {
		AssignFloor(-1, "preset_lshape_double_bg+", direction);
		AssignRoom(-1, 0, "room_residence", kComponent, kComponentId);
		AssignRoom(-1, 1, "room_residence", kComponent, kComponentId);
		ArrangeRow(-1, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 3, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 4, "room_residence", size, kComponent, kComponentId);
		AssignFloor(0, "preset_lshape_double_fg+-", direction);
		AssignRoom(0, 0, "room_residence", kComponent, kComponentId);
		AssignRoom(0, 1, "room_residence", kComponent, kComponentId);
		ArrangeRow(0, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 3, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 4, "room_residence", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_lshape_double_fu+-", direction);
			AssignRoom(i, 0, "room_residence", kComponent, kComponentId);
			AssignRoom(i, 1, "room_residence", kComponent, kComponentId);
			ArrangeRow(i, 0, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 2, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 3, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 4, "room_residence", size, kComponent, kComponentId);
		}
	} else { // layout == 3
		AssignFloor(-1, "preset_nshape_double_bg+", direction);
		AssignRoom(-1, 0, "room_empty", kComponent, kComponentId); // 占位槽位，不铺真实房间
		AssignRoom(-1, 1, "room_residence", kComponent, kComponentId);
		AssignRoom(-1, 2, "room_residence", kComponent, kComponentId);
		ArrangeRow(-1, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 3, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 4, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 5, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 6, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(-1, 7, "room_residence", size, kComponent, kComponentId);
		AssignFloor(0, "preset_nshape_double_fg+-", direction);
		AssignRoom(0, 0, "room_empty", kComponent, kComponentId);
		AssignRoom(0, 1, "room_residence", kComponent, kComponentId);
		AssignRoom(0, 2, "room_residence", kComponent, kComponentId);
		ArrangeRow(0, 0, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 1, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 2, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 3, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 4, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 5, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 6, "room_residence", size, kComponent, kComponentId);
		ArrangeRow(0, 7, "room_residence", size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_nshape_double_fu+-", direction);
			AssignRoom(i, 0, "room_empty", kComponent, kComponentId);
			AssignRoom(i, 1, "room_residence", kComponent, kComponentId);
			AssignRoom(i, 2, "room_residence", kComponent, kComponentId);
			ArrangeRow(i, 0, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 1, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 2, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 3, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 4, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 5, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 6, "room_residence", size, kComponent, kComponentId);
			ArrangeRow(i, 7, "room_residence", size, kComponent, kComponentId);
		}
		AssignElevatorCabin(0, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
		AssignElevatorCabin(1, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	}
}

void ResidenceLowBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

float ResidenceLowBuilding::RandomAcreage() {
	return 2000.f * powf(1.f + GetRandom(1000) / 1000.f * 2.f, 2);
}

float ResidenceLowBuilding::GetAcreageMin() {
	return 2000.f;
}

float ResidenceLowBuilding::GetAcreageMax() {
	return 18000.f;
}

float ResidenceLowBuilding::GetPower(AREA_TYPE area) {
	// 只在住宅低密度分区上有权重，和其余类型的分区互斥，见building_basic.md。
	if (area == AREA_RESIDENTIAL_LOW) {
		return 1.f;
	}
	return 0.f;
}

int ShopHighBuilding::count = 0;

ShopHighBuilding::ShopHighBuilding() : id(count++) {
}

const char* ShopHighBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void ShopHighBuilding::Layout(int& direction, const Quad& quad,
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

void ShopHighBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

float ShopHighBuilding::RandomAcreage() {
	return 4000.f * powf(1.f + GetRandom(1000) / 1000.f * 1.f, 2);
}

float ShopHighBuilding::GetAcreageMin() {
	return 4000.f;
}

float ShopHighBuilding::GetAcreageMax() {
	return 16000.f;
}

float ShopHighBuilding::GetPower(AREA_TYPE area) {
	// 只在商店高密度分区上有权重，和其余类型的分区互斥，见building_basic.md。
	if (area == AREA_COMMERCIAL_HIGH) {
		return 1.f;
	}
	return 0.f;
}

int ShopMiddleBuilding::count = 0;

ShopMiddleBuilding::ShopMiddleBuilding() : id(count++) {
}

const char* ShopMiddleBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void ShopMiddleBuilding::Layout(int& direction, const Quad& quad,
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

void ShopMiddleBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

float ShopMiddleBuilding::RandomAcreage() {
	return 4000.f * powf(1.f + GetRandom(1000) / 1000.f * 1.f, 2);
}

float ShopMiddleBuilding::GetAcreageMin() {
	return 4000.f;
}

float ShopMiddleBuilding::GetAcreageMax() {
	return 16000.f;
}

float ShopMiddleBuilding::GetPower(AREA_TYPE area) {
	// 只在商店中密度分区上有权重，和其余类型的分区互斥，见building_basic.md。
	if (area == AREA_COMMERCIAL_MIDDLE) {
		return 1.f;
	}
	return 0.f;
}

int ShopLowBuilding::count = 0;

ShopLowBuilding::ShopLowBuilding() : id(count++) {
}

const char* ShopLowBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void ShopLowBuilding::Layout(int& direction, const Quad& quad,
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

void ShopLowBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

float ShopLowBuilding::RandomAcreage() {
	return 4000.f * powf(1.f + GetRandom(1000) / 1000.f * 1.f, 2);
}

float ShopLowBuilding::GetAcreageMin() {
	return 4000.f;
}

float ShopLowBuilding::GetAcreageMax() {
	return 16000.f;
}

float ShopLowBuilding::GetPower(AREA_TYPE area) {
	// 只在商店低密度分区上有权重，和其余类型的分区互斥，见building_basic.md。
	if (area == AREA_COMMERCIAL_LOW) {
		return 1.f;
	}
	return 0.f;
}

int FactoryHighBuilding::count = 0;

FactoryHighBuilding::FactoryHighBuilding() : id(count++) {
}

const char* FactoryHighBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void FactoryHighBuilding::Layout(int& direction, const Quad& quad,
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

	constexpr const char* kComponent = "component_factory";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.wallMaterial = "/Game/Asset/Building/Material/White.White";

	// 地上固定1层，显式赋值不依赖BuildingMod基类默认值。
	layers = 1;
	floorHeights.assign(basements + layers, 0.6f);

	if (basements > 0) {
		AssignFloor(-1, "preset_single_room_pg+", direction, assets);
		AssignRoom(-1, 0, "room_parking", kComponent, kComponentId);
		AssignFloor(0, "preset_single_room_fg-", direction, assets);
	} else {
		AssignFloor(0, "preset_single_room_fg", direction, assets);
	}
	// 三种工坊房间等概率随机选一种：小麦/牛肉/汉堡工厂。
	static const char* const kFactoryRooms[] = { "room_farm", "room_ranch", "room_food_factory" };
	AssignRoom(0, 0, kFactoryRooms[GetRandom(3)], kComponent, kComponentId);
}

void FactoryHighBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

// 公式基数2000和GetAcreageMin的4000不一致(RandomAcreage可能小于Min)，是既有数值。
float FactoryHighBuilding::RandomAcreage() {
	return 2000.f * powf(1.f + GetRandom(1000) / 1000.f * 1.f, 2);
}

float FactoryHighBuilding::GetAcreageMin() {
	return 4000.f;
}

float FactoryHighBuilding::GetAcreageMax() {
	return 16000.f;
}

float FactoryHighBuilding::GetPower(AREA_TYPE area) {
	// 只在工厂高密度分区上有权重，和其余类型的分区互斥，见building_basic.md。
	if (area == AREA_INDUSTRIAL_HIGH) {
		return 1.f;
	}
	return 0.f;
}

int FactoryMiddleBuilding::count = 0;

FactoryMiddleBuilding::FactoryMiddleBuilding() : id(count++) {
}

const char* FactoryMiddleBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void FactoryMiddleBuilding::Layout(int& direction, const Quad& quad,
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

	constexpr const char* kComponent = "component_factory";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.wallMaterial = "/Game/Asset/Building/Material/White.White";

	// 地上固定1层，显式赋值不依赖BuildingMod基类默认值。
	layers = 1;
	floorHeights.assign(basements + layers, 0.6f);

	if (basements > 0) {
		AssignFloor(-1, "preset_single_room_pg+", direction, assets);
		AssignRoom(-1, 0, "room_parking", kComponent, kComponentId);
		AssignFloor(0, "preset_single_room_fg-", direction, assets);
	} else {
		AssignFloor(0, "preset_single_room_fg", direction, assets);
	}
	// 三种工坊房间等概率随机选一种：小麦/牛肉/汉堡工厂。
	static const char* const kFactoryRooms[] = { "room_farm", "room_ranch", "room_food_factory" };
	AssignRoom(0, 0, kFactoryRooms[GetRandom(3)], kComponent, kComponentId);
}

void FactoryMiddleBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

// 公式基数2000和GetAcreageMin的4000不一致(RandomAcreage可能小于Min)，是既有数值。
float FactoryMiddleBuilding::RandomAcreage() {
	return 2000.f * powf(1.f + GetRandom(1000) / 1000.f * 1.f, 2);
}

float FactoryMiddleBuilding::GetAcreageMin() {
	return 4000.f;
}

float FactoryMiddleBuilding::GetAcreageMax() {
	return 16000.f;
}

float FactoryMiddleBuilding::GetPower(AREA_TYPE area) {
	// 只在工厂中密度分区上有权重，和其余类型的分区互斥，见building_basic.md。
	if (area == AREA_INDUSTRIAL_MIDDLE) {
		return 1.f;
	}
	return 0.f;
}

int FactoryLowBuilding::count = 0;

FactoryLowBuilding::FactoryLowBuilding() : id(count++) {
}

const char* FactoryLowBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void FactoryLowBuilding::Layout(int& direction, const Quad& quad,
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

	constexpr const char* kComponent = "component_factory";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.wallMaterial = "/Game/Asset/Building/Material/White.White";

	// 地上固定1层，显式赋值不依赖BuildingMod基类默认值。
	layers = 1;
	floorHeights.assign(basements + layers, 0.6f);

	if (basements > 0) {
		AssignFloor(-1, "preset_single_room_pg+", direction, assets);
		AssignRoom(-1, 0, "room_parking", kComponent, kComponentId);
		AssignFloor(0, "preset_single_room_fg-", direction, assets);
	} else {
		AssignFloor(0, "preset_single_room_fg", direction, assets);
	}
	// 三种工坊房间等概率随机选一种：小麦/牛肉/汉堡工厂。
	static const char* const kFactoryRooms[] = { "room_farm", "room_ranch", "room_food_factory" };
	AssignRoom(0, 0, kFactoryRooms[GetRandom(3)], kComponent, kComponentId);
}

void FactoryLowBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

// 公式基数2000和GetAcreageMin的4000不一致(RandomAcreage可能小于Min)，是既有数值。
float FactoryLowBuilding::RandomAcreage() {
	return 2000.f * powf(1.f + GetRandom(1000) / 1000.f * 1.f, 2);
}

float FactoryLowBuilding::GetAcreageMin() {
	return 4000.f;
}

float FactoryLowBuilding::GetAcreageMax() {
	return 16000.f;
}

float FactoryLowBuilding::GetPower(AREA_TYPE area) {
	// 只在工厂低密度分区上有权重，和其余类型的分区互斥，见building_basic.md。
	if (area == AREA_INDUSTRIAL_LOW) {
		return 1.f;
	}
	return 0.f;
}

int OfficeHighBuilding::count = 0;

OfficeHighBuilding::OfficeHighBuilding() : id(count++) {
}

const char* OfficeHighBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void OfficeHighBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

float OfficeHighBuilding::RandomAcreage() {
	return 2000.f * powf(1.f + GetRandom(1000) / 1000.f * 2.f, 2);
}

float OfficeHighBuilding::GetAcreageMin() {
	return 2000.f;
}

float OfficeHighBuilding::GetAcreageMax() {
	return 18000.f;
}

void OfficeHighBuilding::Layout(int& direction, const Quad& quad,
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

float OfficeHighBuilding::GetPower(AREA_TYPE area) {
	// 只在办公楼高密度分区上有权重，和其余类型的分区互斥，见building_basic.md。
	if (area == AREA_OFFICIAL_HIGH) {
		return 1.f;
	}
	return 0.f;
}

int OfficeMiddleBuilding::count = 0;

OfficeMiddleBuilding::OfficeMiddleBuilding() : id(count++) {
}

const char* OfficeMiddleBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void OfficeMiddleBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

float OfficeMiddleBuilding::RandomAcreage() {
	return 2000.f * powf(1.f + GetRandom(1000) / 1000.f * 2.f, 2);
}

float OfficeMiddleBuilding::GetAcreageMin() {
	return 2000.f;
}

float OfficeMiddleBuilding::GetAcreageMax() {
	return 18000.f;
}

void OfficeMiddleBuilding::Layout(int& direction, const Quad& quad,
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

float OfficeMiddleBuilding::GetPower(AREA_TYPE area) {
	// 只在办公楼中密度分区上有权重，和其余类型的分区互斥，见building_basic.md。
	if (area == AREA_OFFICIAL_MIDDLE) {
		return 1.f;
	}
	return 0.f;
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
	// 只在办公楼低密度分区上有权重，和其余类型的分区互斥，见building_basic.md。
	if (area == AREA_OFFICIAL_LOW) {
		return 1.f;
	}
	return 0.f;
}

int TrainStationBuilding::count = 0;

TrainStationBuilding::TrainStationBuilding() : id(count++) {
}

const char* TrainStationBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void TrainStationBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 楼体占满整个Building区域(不像机场只占贴路一侧)，TrainStation::Layout的两条轨道/四个接口
	// (depthRatio 0.55/0.65)因此落在楼体范围之内，见station_basic.md。
	stationMod = "station_train";
	footprint = BuildingFootprintSpec();   // 默认就是中心(0.5,0.5)、占满(1,1)
	basements = 1;
	layers = 1;
	floorHeights.assign(basements + layers, 0.6f);

	constexpr const char* kComponent = "component_train_station";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.stairMeshPath = "/Game/Asset/Building/Mesh/Stair_Line.Stair_Line";
	// 地下1层(站台层，楼梯通上来)+地面1层(大厅，左右两间站房)，两层各有2台电梯
	AssignFloor(-1, "preset_train_station_bg+", direction, assets);
	AssignFloor(0, "preset_train_station_fg-", direction, assets);
	AssignRoom(0, 0, "room_train_station", kComponent, kComponentId);
	AssignRoom(0, 1, "room_train_station", kComponent, kComponentId);
	AssignElevatorCabin(0, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	AssignElevatorCabin(1, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
}

void TrainStationBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 在井字中心lot(AREA_TEST)上找一块地方，和机场/园区共存：用
	// Lot::FindAdaptivePlacement按当前剩余自由空间自适应选，依次尝试4个方向，先找到可用空间的方向就用。
	constexpr float kMinFrontage = 7.f;
	constexpr float kMinDepth = 4.f;

	for (Lot* lot : lots) {
		if (!lot || lot->GetArea() != AREA_TEST) continue;

		for (int dir = FACE_WEST; dir <= FACE_SOUTH; dir++) {
			Lot* targetLot;
			float marginStart, marginEnd, depth;
			if (!lot->FindAdaptivePlacement(dir, kMinFrontage, kMinDepth, targetLot, marginStart, marginEnd, depth)) continue;

			// FindAdaptivePlacement给的是居中的margin：余量全挪到marginEnd，让火车站贴在这条边的起点角。
			marginEnd += marginStart;
			marginStart = 0.f;

			// request.lot是命中的自由子块targetLot而不是lot本身：margin/depth是相对子块算的，
			// 要在子块自己的坐标系里裁剪。adaptiveParent仅在targetLot!=lot时设，Core处理成功后
			// 靠它把子块的残余空间/新增小路搬回lot(不能在Basic.dll里搬)，见FindAdaptivePlacement声明。
			LotPlacementRequest request;
			request.lot = targetLot;
			request.adaptiveParent = (targetLot != lot) ? lot : nullptr;
			request.direction = dir;
			request.marginStart = marginStart;
			request.marginEnd = marginEnd;
			request.depth = depth;
			emit(context, request);
			return;
		}
		return; // 中心lot只有一个，4个方向都试过就不用再看其它lot
	}
}

float TrainStationBuilding::RandomAcreage() { return 2000.f; }
float TrainStationBuilding::GetAcreageMin() { return 2000.f; }
float TrainStationBuilding::GetAcreageMax() { return 2000.f; }
float TrainStationBuilding::GetPower(AREA_TYPE area) { return 0.f; }

int AirportBuilding::count = 0;

AirportBuilding::AirportBuilding() : id(count++) {
}

const char* AirportBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void AirportBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 楼体贴路一侧(NearRoadFootprint)，深处留给AirStation::Layout的两条跑道(depthRatio 0.65/0.85)。
	stationMod = "station_air";
	footprint = NearRoadFootprint(direction, 0.18f, 0.35f, 0.6f);
	basements = 0;
	layers = 1;
	floorHeights.assign(1, 0.6f);

	constexpr const char* kComponent = "component_airport";
	constexpr int kComponentId = 0;
	AssignFloor(0, "preset_single_room_fg", direction);
	AssignRoom(0, 0, "room_airport", kComponent, kComponentId);
}

void AirportBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 同TrainStationBuilding::Assign的自适应思路：FindAdaptivePlacement会看到先处理的建筑留下的最新
	// freeLots状态(处理顺序由BuildingFactory::GetRegisteredIds()决定，这里不关心)。
	// 占地(沿路x纵深，地图单位)优先12x8(要放跑道和两侧停机位)，放不下退回和火车站一样的7x4。
	constexpr float kFrontages[] = { 12.f, 7.f };
	constexpr float kDepths[] = { 8.f, 4.f };

	for (Lot* lot : lots) {
		if (!lot || lot->GetArea() != AREA_TEST) continue;

		for (int size = 0; size < 2; size++)
		for (int dir = FACE_WEST; dir <= FACE_SOUTH; dir++) {
			Lot* targetLot;
			float marginStart, marginEnd, depth;
			if (!lot->FindAdaptivePlacement(dir, kFrontages[size], kDepths[size], targetLot, marginStart, marginEnd, depth)) continue;

			// 余量全挪到marginStart，贴另一端的角(和火车站相反)，两者同在一条边时占两个对角。
			marginStart += marginEnd;
			marginEnd = 0.f;

			LotPlacementRequest request;
			request.lot = targetLot;
			request.adaptiveParent = (targetLot != lot) ? lot : nullptr;
			request.direction = dir;
			request.marginStart = marginStart;
			request.marginEnd = marginEnd;
			request.depth = depth;
			emit(context, request);
			return;
		}
		return;
	}
}

float AirportBuilding::RandomAcreage() { return 2000.f; }
float AirportBuilding::GetAcreageMin() { return 2000.f; }
float AirportBuilding::GetAcreageMax() { return 2000.f; }
float AirportBuilding::GetPower(AREA_TYPE area) { return 0.f; }

// 公园种树的mesh(和行道树同一棵测试树)和密度：每PARK_ELEMENTS_PER_TREE个element²一棵(约每200m²)。
#define PARK_TREE_MESH_PATH "/Game/3rdParty/Mossy_Tree/StaticMeshes/mossy-tree.mossy-tree"
#define PARK_TREE_SCALE 1.f
#define PARK_ELEMENTS_PER_TREE 2.f

int ParkBuilding::count = 0;

ParkBuilding::ParkBuilding() : id(count++) {
}

const char* ParkBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void ParkBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 0层：Core不建Floor，UE层不生成楼体(见building.md"允许0层建筑")。
	basements = 0;
	layers = 0;
	floorHeights.clear();
}

void ParkBuilding::LayoutTrees(const Quad& quad, const std::unordered_map<int, Road*>& boundaryRoads) {
	int num = max(1, static_cast<int>(quad.GetSizeX() * quad.GetSizeY() / PARK_ELEMENTS_PER_TREE));
	// 建筑植被参考系是整块quad：两个对角(0,0)-(1,1)就是整块地。
	AddTreeArea({ 0.f, 0.f, 0.f, 0.f }, { 1.f, 0.f, 1.f, 0.f }, num, PARK_TREE_SCALE, PARK_TREE_MESH_PATH);
}

void ParkBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
}

float ParkBuilding::RandomAcreage() {
	return 3000.f + GetRandom(1000) / 1000.f * 5000.f;
}

float ParkBuilding::GetAcreageMin() {
	return 3000.f;
}

float ParkBuilding::GetAcreageMax() {
	return 8000.f;
}

float ParkBuilding::GetPower(AREA_TYPE area) {
	// 绿地分区本来就该是公园；住宅分区给一个较小的权重，让测试时住宅区里也能刷出几块公园
	// (现在的井字路网不产出AREA_GREEN的lot)。
	if (area == AREA_GREEN) return 1.f;
	return 0.f;
}

// Mohar模型和它的默认摆位(先给默认值，看过效果再调)：位置是楼体局部的PointParams
// (xRatio,xOffset,yRatio,yOffset)，按face=2(朝北)定义；Z是离地高度(地图单位)；缩放是资产原尺寸
// 的倍数；YAW是度，同样按朝北定义——实际朝向不同时Core会自动旋转，见feature_spec.h的DecorationSpec。
#define MOHAR_MESH_PATH "/Game/3rdParty/Mohar/StaticMeshes/mohar.mohar"
#define MOHAR_POS_X_RATIO 0.5f
#define MOHAR_POS_X_OFFSET 0.f
#define MOHAR_POS_Y_RATIO 0.5f
#define MOHAR_POS_Y_OFFSET 0.f
#define MOHAR_Z 0.f
#define MOHAR_SCALE_XY 0.06f
#define MOHAR_SCALE_Z 0.04f
// 模型初始旋转：资产自身的大门朝向和楼层模板的约定差90度，逆时针转90度(俯视)之后，朝北(face=2)
// 时模型大门也朝北，和模板一致。UE的正yaw俯视是顺时针，所以逆时针90度是-90。
#define MOHAR_YAW -90.f
// 楼体footprint的四条边(地图单位，1=10m)，按"朝北"(face=2，模板原样)定义、相对模型中心(=地块
// 中心)：+X是东、+Y是南(大门在北墙)，所以WEST/NORTH是负数。其余朝向按InversePoint同一套旋转换算，
// 和模型、楼层模板一起转；模型本身不动。layout模板3rdparty_mohar_fg1按这个footprint拟合一楼范围。
// 数值是按模型实测调出来的。
#define MOHAR_BODY_WEST -1.55f
#define MOHAR_BODY_EAST 1.25f
#define MOHAR_BODY_NORTH -0.95f
#define MOHAR_BODY_SOUTH 1.5f
// 层高(地图单位，0.55=5.5m)：一楼5.5m，二楼4m，三楼先按4m试。
#define MOHAR_FLOOR1_HEIGHT 0.55f
#define MOHAR_FLOOR2_HEIGHT 0.4f
#define MOHAR_FLOOR3_HEIGHT 0.4f
#define MOHAR_FLOOR1_TEMPLATE "3rdparty_mohar_fg1"
// 一楼用fg1，二楼用fu2，三楼用fu3，各层模板独立。
#define MOHAR_FLOOR2_TEMPLATE "3rdparty_mohar_fu2"
#define MOHAR_FLOOR3_TEMPLATE "3rdparty_mohar_fu3"
// 每层要不要由框架构造墙壁/地板/天花板(FloorAssetSpec::buildShell)：成品模型已经包含这些，三层
// 都关掉；房间/Room碰撞盒照常生成。核对某一层的房间范围时把那一层临时改成true就能看到框架画的楼层。
#define MOHAR_FLOOR1_BUILD_SHELL false
#define MOHAR_FLOOR2_BUILD_SHELL false
#define MOHAR_FLOOR3_BUILD_SHELL false

int MoharBuilding::count = 0;

MoharBuilding::MoharBuilding() : id(count++) {
}

const char* MoharBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void MoharBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 和ResidenceBuilding一样：FillRemainder落地传进来的是-1，模型朝向要靠direction决定，
	// 所以从有真实边界路的方向里随机挑一个回写。
	if (direction < 0) {
		std::vector<int> candidates;
		for (auto& [dir, road] : boundaryRoads) {
			if (road) candidates.push_back(dir);
		}
		if (!candidates.empty()) {
			direction = candidates[GetRandom(static_cast<int>(candidates.size()))];
		}
	}

	// 各层用3rdparty_mohar_fg1/fu2/fu3模板拟合模型里的房间；楼体外观由下面的成品模型提供，每层的
	// buildShell控制框架要不要再构造近处的墙壁/地板/天花板(远处方块照常画)，见MOHAR_FLOOR*_BUILD_SHELL。
	int face = direction >= 0 ? direction : FACE_NORTH;
	// 朝北坐标系里的楼体尺寸和中心(相对模型)，再换算到实际朝向：尺寸在朝东/西时宽和进深互换，
	// 中心位移和InversePoint对offset的映射一致：face0 (dx,dy)->(dy,-dx)，face1 ->(-dy,dx)，
	// face3 ->(-dx,-dy)。
	const float northSizeX = MOHAR_BODY_EAST - MOHAR_BODY_WEST;
	const float northSizeY = MOHAR_BODY_SOUTH - MOHAR_BODY_NORTH;
	const float northCenterX = (MOHAR_BODY_WEST + MOHAR_BODY_EAST) * 0.5f;
	const float northCenterY = (MOHAR_BODY_NORTH + MOHAR_BODY_SOUTH) * 0.5f;
	bool axesSwapped = (face == FACE_WEST || face == FACE_EAST);
	float bodySizeX = axesSwapped ? northSizeY : northSizeX;
	float bodySizeY = axesSwapped ? northSizeX : northSizeY;
	float shiftX = northCenterX, shiftY = northCenterY;
	switch (face) {
	case FACE_WEST: { float t = shiftX; shiftX = shiftY; shiftY = -t; break; }
	case FACE_EAST: { float t = shiftX; shiftX = -shiftY; shiftY = t; break; }
	case FACE_SOUTH: shiftX = -shiftX; shiftY = -shiftY; break;
	default: break;
	}
	footprint.centerRatioX = quad.GetSizeX() > 0.f ? 0.5f + shiftX / quad.GetSizeX() : 0.5f;
	footprint.centerRatioY = quad.GetSizeY() > 0.f ? 0.5f + shiftY / quad.GetSizeY() : 0.5f;
	footprint.sizeRatioX = quad.GetSizeX() > 0.f ? min(1.f, bodySizeX / quad.GetSizeX()) : 1.f;
	footprint.sizeRatioY = quad.GetSizeY() > 0.f ? min(1.f, bodySizeY / quad.GetSizeY()) : 1.f;

	basements = 0;
	layers = 3;
	floorHeights = { MOHAR_FLOOR1_HEIGHT, MOHAR_FLOOR2_HEIGHT, MOHAR_FLOOR3_HEIGHT };
	// 各层的房间挂在同一个component下(一栋独栋住宅)。
	const char* floorTemplates[3] = { MOHAR_FLOOR1_TEMPLATE, MOHAR_FLOOR2_TEMPLATE, MOHAR_FLOOR3_TEMPLATE };
	const bool floorBuildShell[3] = { MOHAR_FLOOR1_BUILD_SHELL, MOHAR_FLOOR2_BUILD_SHELL, MOHAR_FLOOR3_BUILD_SHELL };
	for (int level = 0; level < 3; level++) {
		FloorAssetSpec assets;
		assets.buildShell = floorBuildShell[level];
		AssignFloor(level, floorTemplates[level], face, assets);
		AssignRoom(level, 0, "room_residence", "component_residence", 0);
	}

	DecorationSpec spec;
	spec.meshPath = MOHAR_MESH_PATH;
	// 模型位置是相对楼体中心的：楼体中心相对模型偏了(northCenterX, northCenterY)，模型反向补回，
	// 保持在地块中心不动(InversePoint按朝向旋转这个offset，和楼体位移同一套映射)。
	spec.position = { MOHAR_POS_X_RATIO, MOHAR_POS_X_OFFSET - northCenterX, MOHAR_POS_Y_RATIO, MOHAR_POS_Y_OFFSET - northCenterY };
	spec.z = MOHAR_Z;
	spec.scaleX = spec.scaleY = MOHAR_SCALE_XY;
	spec.scaleZ = MOHAR_SCALE_Z;
	spec.yaw = MOHAR_YAW;
	AddDecoration(spec);
}

void MoharBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
}

float MoharBuilding::RandomAcreage() {
	return 3000.f + GetRandom(1000) / 1000.f * 5000.f;
}

float MoharBuilding::GetAcreageMin() {
	return 3000.f;
}

float MoharBuilding::GetAcreageMax() {
	return 8000.f;
}

float MoharBuilding::GetPower(AREA_TYPE area) {
	if (area == AREA_RESIDENTIAL_HIGH) {
		return 0.5f;
	}
	return 0.f;
}
