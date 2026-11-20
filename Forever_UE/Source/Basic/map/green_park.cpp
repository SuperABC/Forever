#include "green_park.h"

#include <algorithm>
#include <string>


using namespace std;

// 公园种树的mesh(和行道树同一棵测试树)和密度：每PARK_ELEMENTS_PER_TREE个element²一棵(约每200m²)。
#define PARK_TREE_MESH_PATH "/Game/3rdParty/Mossy_Tree/StaticMeshes/mossy-tree.mossy-tree"
#define PARK_TREE_SCALE 1.f
#define PARK_ELEMENTS_PER_TREE 2.f
// 活动圆取最大内切圆的这个比例(1=贴着多边形边界)。
#define PARK_CIRCLE_SCALE 1.f

int ParkGreen::count = 0;

ParkGreen::ParkGreen() : id(count++) {
}

const char* ParkGreen::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void ParkGreen::LayoutGreen() {
	// 只放一个活动圆：多边形能容纳的最大圆。
	float cx, cy, r;
	if (FindLargestCircle(cx, cy, r)) AddCircle(cx, cy, r * PARK_CIRCLE_SCALE);

	// 树：在包围盒里随机撒，Core会丢掉多边形外的点，所以按包围盒面积算数量，多边形内的密度才是设定值。
	float boxArea = (maxX - minX) * (maxY - minY);
	int num = max(1, static_cast<int>(boxArea / PARK_ELEMENTS_PER_TREE));
	AddTreeArea({ 0.f, 0.f, 0.f, 0.f }, { 1.f, 0.f, 1.f, 0.f }, num, PARK_TREE_SCALE, PARK_TREE_MESH_PATH);
}

float ParkGreen::GetPowerFree() {
	return 1.f;
}

float ParkGreen::GetPowerLinked() {
	return 1.f;
}
