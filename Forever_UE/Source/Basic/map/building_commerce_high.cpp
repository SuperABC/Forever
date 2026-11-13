#include "building_commerce_high.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>


using namespace std;

namespace {
	constexpr const char* kComponent = "component_residence";
}

int CommerceHighBuilding::count = 0;

CommerceHighBuilding::CommerceHighBuilding() : id(count++) {
}

const char* CommerceHighBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void CommerceHighBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
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
	// 偏好方向：长边朝向正对的路，只有boundaryRoads里确实有非空条目时才采用，否则保留上面兜底出来的direction。
	int preferredDirection = quad.GetSizeX() > quad.GetSizeY() ? FACE_NORTH + GetRandom(2) : FACE_WEST + GetRandom(2);
	auto roadIt = boundaryRoads.find(preferredDirection);
	if (roadIt != boundaryRoads.end() && roadIt->second) {
		direction = preferredDirection;
	}

	// 楼体占满地块，四周各留固定kSideGap的空隙(空隙铺gapMaterial)；地块太小时退化成居中的最小尺寸。
	constexpr float kSideGap = 0.2f;
	const float quadX = quad.GetSizeX(), quadY = quad.GetSizeY();
	footprint = BuildingFootprintSpec{ 0.5f, 0.5f,
		max(0.f, quadX - 2.f * kSideGap) / quadX, max(0.f, quadY - 2.f * kSideGap) / quadY };
	// common_tshape：没有地下室，地上3~5层随机(1楼fg+、其余fu+-)。
	basements = 0;
	layers = 3 + GetRandom(3);
	floorHeights.clear();
	for (int i = 0; i < layers; i++) {
		floorHeights.push_back(0.4f);
	}
	// lodMaterial留空 -> 用渲染层默认灰色Pure MID
	SetSurfaceMaterial("clay", "/Game/Asset/Building/Material/Clay.Clay");
	SetSurfaceMaterial("concrete", "/Game/Asset/Building/Material/Concrete.Concrete");
	// 楼体与建筑范围之间的空隙铺人行道材质。
	gapMaterial = "/Game/Asset/Building/Material/Pavement.Pavement";

	constexpr const char* kComponent = "component_shop";
	constexpr float size = 120.f; // 联排房间面积

	// 每层一个component(componentId=层号)：1个single(room_empty占位)+6个row，row 0~3是营业房
	// (整栋都是商店)，row 4~5是仓库；2台电梯贯穿全楼。
	for (int level = 0; level < layers; level++) {
		AssignFloor(level, level == 0 ? "preset_common_tshape_fg+" : "preset_common_tshape_fu+-", direction);
		AssignRoom(level, 0, "room_empty", kComponent, level);
		for (int row = 0; row < 6; row++) {
			const char* room = row >= 4 ? "room_warehouse" : "room_shop";
			ArrangeRow(level, row, room, size, kComponent, level);
		}
	}
	for (int elevator = 0; elevator < 2; elevator++) {
		AssignElevatorCabin(elevator, 0, layers - 1, "", "script_empty", { "elevator_basic" });
	}
}

void CommerceHighBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 不用显式占位。
}

float CommerceHighBuilding::RandomAcreage() {
	return 8000.f + 8000.f * GetRandom(1000) / 1000.f; // 8000~16000线性随机
}

float CommerceHighBuilding::GetAcreageMin() {
	return 8000.f;
}

float CommerceHighBuilding::GetAcreageMax() {
	return 16000.f;
}

float CommerceHighBuilding::GetPower(AREA_TYPE area) {
	// 只在商店高密度分区上有权重，和其余类型的分区互斥，见building_commerce_high.md。
	if (area == AREA_COMMERCE_HIGH) {
		return 1.f;
	}
	return 0.f;
}
