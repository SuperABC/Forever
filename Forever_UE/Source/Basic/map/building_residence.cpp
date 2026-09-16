#include "building_residence.h"
#include "room_residence.h"

#include "common/utility.h"

#include <cmath>

using namespace std;

int ResidenceBuilding::count = 0;

ResidenceBuilding::ResidenceBuilding() {
	lastName = string(GetType()) + std::to_string(count++);
}

void ResidenceBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// direction对显式占位是真实方向，对权重CDF/FillRemainder落地传进来是-1——这个类型的
	// 楼层内部布局(AssignFloor选的模板)按direction摆朝向，所以-1时要从boundaryRoads里随机
	// 挑一个有真实边界路的方向回写，保证这栋building最终有确定方向可用(行人/车辆导航的
	// "outside"端点要靠这个方向找到该连去哪条路，见building_mod.h的Layout()注释)。
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
	// 楼层数分档照抄老工程ResidentialBuilding::LayoutBuilding原始数值。
	if (quad.GetAcreage() < 5000.f) {
		layers = 3 + GetRandom(3);
	} else if (quad.GetAcreage() < 10000.f) {
		layers = 6 + GetRandom(4);
	} else {
		layers = 10 + GetRandom(5);
	}
	floorHeights.clear();
	floorHeights.push_back(0.35f); // 地下室
	// "每层高度循环取值、不完全一样"这个特性已经验证过(以及验证过程中发现并修复的"循环表
	// 固定相位导致全地图第3层系统性偏高"那个问题)，现在恢复成地上每层统一0.4，不再循环取值。
	for (int i = 0; i < layers; i++) {
		floorHeights.push_back(0.4f);
	}
	// lodMaterial留空 -> 用渲染层默认灰色Pure MID

	constexpr const char* kComponent = "component_residence";
	constexpr int kComponentId = 0;

	// 按quad尺寸分档随机选一套模板组合(layout 0-3)+对应的房间槽位密度(size)——完整照抄老工程
	// ResidentialBuilding::LayoutBuilding的选型逻辑(building_basic.cpp:47-199)，layout==1/3
	// 自带电梯井道模板数据(preset_lobby_wing_*/preset_nshape_double_*)，这次会话新增的
	// AssignElevatorCabin就是靠这两个分支才有真正的Elevator几何可用。
	//
	// 方向：老工程这里直接用quad尺寸/朝向算出的"偏好方向"覆盖掉一开始GetRandom(4)的初始值，
	// 不检查这个方向是否真的有边界路——这次不能照抄这一点，偏好方向如果没有对应的真实边界路，
	// 会导致上面费心兜底出来的、行人/车辆导航outside端点能用的direction被覆盖成一个查不到路
	// 的方向。这里做法：只有偏好方向在boundaryRoads里确实有非空条目时才采用，否则保留上面
	// 已经兜底好的direction。
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
		ArrangeRow(-1, 0, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(-1, 1, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		AssignFloor(0, "preset_straight_linear_fg+-", direction);
		ArrangeRow(0, 0, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(0, 1, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_straight_linear_fu+-", direction);
			ArrangeRow(i, 0, ResidenceRoom::GetId(), size, kComponent, kComponentId);
			ArrangeRow(i, 1, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		}
	} else if (layout == 1) {
		AssignFloor(-1, "preset_lobby_wing_bg+", direction);
		ArrangeRow(-1, 0, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(-1, 1, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(-1, 2, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(-1, 3, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		AssignFloor(0, "preset_lobby_wing_fg+-", direction);
		ArrangeRow(0, 0, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(0, 1, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(0, 2, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(0, 3, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_lobby_wing_fu+-", direction);
			ArrangeRow(i, 0, ResidenceRoom::GetId(), size, kComponent, kComponentId);
			ArrangeRow(i, 1, ResidenceRoom::GetId(), size, kComponent, kComponentId);
			ArrangeRow(i, 2, ResidenceRoom::GetId(), size, kComponent, kComponentId);
			ArrangeRow(i, 3, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		}
		AssignElevatorCabin(0, -basements, layers - 1, "");
		AssignElevatorCabin(1, -basements, layers - 1, "");
	} else if (layout == 2) {
		AssignFloor(-1, "preset_lshape_double_bg+", direction);
		AssignRoom(-1, 0, ResidenceRoom::GetId(), kComponent, kComponentId);
		AssignRoom(-1, 1, ResidenceRoom::GetId(), kComponent, kComponentId);
		ArrangeRow(-1, 0, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(-1, 1, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(-1, 2, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(-1, 3, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(-1, 4, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		AssignFloor(0, "preset_lshape_double_fg+-", direction);
		AssignRoom(0, 0, ResidenceRoom::GetId(), kComponent, kComponentId);
		AssignRoom(0, 1, ResidenceRoom::GetId(), kComponent, kComponentId);
		ArrangeRow(0, 0, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(0, 1, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(0, 2, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(0, 3, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(0, 4, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_lshape_double_fu+-", direction);
			AssignRoom(i, 0, ResidenceRoom::GetId(), kComponent, kComponentId);
			AssignRoom(i, 1, ResidenceRoom::GetId(), kComponent, kComponentId);
			ArrangeRow(i, 0, ResidenceRoom::GetId(), size, kComponent, kComponentId);
			ArrangeRow(i, 1, ResidenceRoom::GetId(), size, kComponent, kComponentId);
			ArrangeRow(i, 2, ResidenceRoom::GetId(), size, kComponent, kComponentId);
			ArrangeRow(i, 3, ResidenceRoom::GetId(), size, kComponent, kComponentId);
			ArrangeRow(i, 4, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		}
	} else { // layout == 3
		AssignFloor(-1, "preset_nshape_double_bg+", direction);
		AssignRoom(-1, 0, "empty", kComponent, kComponentId); // 老工程这里也是"empty"占位槽位
		AssignRoom(-1, 1, ResidenceRoom::GetId(), kComponent, kComponentId);
		AssignRoom(-1, 2, ResidenceRoom::GetId(), kComponent, kComponentId);
		ArrangeRow(-1, 0, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(-1, 1, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(-1, 2, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(-1, 3, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(-1, 4, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(-1, 5, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(-1, 6, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(-1, 7, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		AssignFloor(0, "preset_nshape_double_fg+-", direction);
		AssignRoom(0, 0, "empty", kComponent, kComponentId);
		AssignRoom(0, 1, ResidenceRoom::GetId(), kComponent, kComponentId);
		AssignRoom(0, 2, ResidenceRoom::GetId(), kComponent, kComponentId);
		ArrangeRow(0, 0, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(0, 1, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(0, 2, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(0, 3, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(0, 4, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(0, 5, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(0, 6, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		ArrangeRow(0, 7, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		for (int i = 1; i < layers; i++) {
			AssignFloor(i, "preset_nshape_double_fu+-", direction);
			AssignRoom(i, 0, "empty", kComponent, kComponentId);
			AssignRoom(i, 1, ResidenceRoom::GetId(), kComponent, kComponentId);
			AssignRoom(i, 2, ResidenceRoom::GetId(), kComponent, kComponentId);
			ArrangeRow(i, 0, ResidenceRoom::GetId(), size, kComponent, kComponentId);
			ArrangeRow(i, 1, ResidenceRoom::GetId(), size, kComponent, kComponentId);
			ArrangeRow(i, 2, ResidenceRoom::GetId(), size, kComponent, kComponentId);
			ArrangeRow(i, 3, ResidenceRoom::GetId(), size, kComponent, kComponentId);
			ArrangeRow(i, 4, ResidenceRoom::GetId(), size, kComponent, kComponentId);
			ArrangeRow(i, 5, ResidenceRoom::GetId(), size, kComponent, kComponentId);
			ArrangeRow(i, 6, ResidenceRoom::GetId(), size, kComponent, kComponentId);
			ArrangeRow(i, 7, ResidenceRoom::GetId(), size, kComponent, kComponentId);
		}
		AssignElevatorCabin(0, -basements, layers - 1, "");
		AssignElevatorCabin(1, -basements, layers - 1, "");
	}
}

void ResidenceBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

// 老工程ResidentialBuilding::RandomAcreage/minAcreage/maxAcreage原始数值，照抄不改。
float ResidenceBuilding::RandomAcreage() {
	return 2000.f * powf(1.f + GetRandom(1000) / 1000.f * 2.f, 2);
}

float ResidenceBuilding::GetAcreageMin() {
	return 2000.f;
}

float ResidenceBuilding::GetAcreageMax() {
	return 18000.f;
}

float ResidenceBuilding::GetPower(AREA_TYPE area) {
	// RoadnetMod(JingRoadnet::DistributeRoadnet)已经会给每个lot调用Lot::SetArea()标好实际的
	// 分区类型，不是AREA_NONE——之前这里错误地假设所有lot都是默认值AREA_NONE，导致GetPower对
	// 每个真实lot都返回0，FillRemainder链路上的候选权重从未被登记过，一个独立building都生成
	// 不出来(PIE验证发现)。这个通用占位类型暂时不按分区类型区分权重，对所有分区一视同仁给
	// 权重1(照抄老工程改造前candidateWeights.push_back({lot,1.f})的行为)，等以后设计具体
	// 建筑类型时再按area细化。
	return 1.f;
}
