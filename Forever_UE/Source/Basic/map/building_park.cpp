#include "building_park.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>


using namespace std;

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
