#include "building_office_high.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>


using namespace std;

namespace {
	constexpr const char* kComponent = "component_residence";
	constexpr int kComponentId = 0;
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
	return 8000.f + 8000.f * GetRandom(1000) / 1000.f; // 8000~16000线性随机
}

float OfficeHighBuilding::GetAcreageMin() {
	return 8000.f;
}

float OfficeHighBuilding::GetAcreageMax() {
	return 16000.f;
}

void OfficeHighBuilding::Layout(int& direction, const Quad& quad,
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
	// 偏好方向：短边朝向正对的路，只有boundaryRoads里确实有非空条目时才采用，否则保留上面兜底出来的direction。
	int preferredDirection = quad.GetSizeX() > quad.GetSizeY() ? FACE_WEST + GetRandom(2) : FACE_NORTH + GetRandom(2);
	auto roadIt = boundaryRoads.find(preferredDirection);
	if (roadIt != boundaryRoads.end() && roadIt->second) {
		direction = preferredDirection;
	}

	footprint = BuildingFootprintSpec{ 0.5f, 0.5f, 0.8f, 0.8f }; // 楼体占地块80%，居中
	// office_circle：地下1层pg+(停车库)，地上2层(1楼fg+-、2楼fu+-)。
	basements = 1;
	layers = 2;
	floorHeights.clear();
	floorHeights.push_back(0.35f); // 地下室
	for (int i = 0; i < layers; i++) {
		floorHeights.push_back(0.4f);
	}
	// lodMaterial留空 -> 用渲染层默认灰色Pure MID
	SetSurfaceMaterial("brick", "/Game/Asset/Building/Material/Brick.Brick");
	SetSurfaceMaterial("pavement", "/Game/Asset/Building/Material/Pavement.Pavement");
	// 楼体与建筑范围之间的空隙铺人行道材质。
	gapMaterial = "/Game/Asset/Building/Material/Pavement.Pavement";

	constexpr const char* kComponent = "component_office";
	constexpr int kComponentId = 0;
	constexpr float size = 120.f; // 联排房间面积

	AssignFloor(-1, "preset_office_circle_pg+", direction);
	AssignRoom(-1, 0, "room_parking", kComponent, kComponentId);
	AssignFloor(0, "preset_office_circle_fg+-", direction); // 4个single(1、2是empty) + 6个row
	for (int slot = 0; slot < 4; slot++) {
		AssignRoom(0, slot, slot == 1 || slot == 2 ? "room_empty" : "room_office", kComponent, kComponentId);
	}
	for (int row = 0; row < 6; row++) {
		ArrangeRow(0, row, "room_office", size, kComponent, kComponentId);
	}
	AssignFloor(1, "preset_office_circle_fu+-", direction); // 5个single(1、2是empty) + 7个row
	for (int slot = 0; slot < 5; slot++) {
		AssignRoom(1, slot, slot == 1 || slot == 2 ? "room_empty" : "room_office", kComponent, kComponentId);
	}
	for (int row = 0; row < 7; row++) {
		ArrangeRow(1, row, "room_office", size, kComponent, kComponentId);
	}
}

float OfficeHighBuilding::GetPower(AREA_TYPE area) {
	// 只在办公楼高密度分区上有权重，和其余类型的分区互斥，见building_office_high.md。
	if (area == AREA_OFFICE_HIGH) {
		return 1.f;
	}
	return 0.f;
}
