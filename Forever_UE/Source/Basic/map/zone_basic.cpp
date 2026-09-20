#include "zone_basic.h"

#include <cmath>


using namespace std;

int ResidenceZone::count = 0;

ResidenceZone::ResidenceZone() {
	lastName = string(GetType()) + std::to_string(count++);
}

// 老工程ResidentialZone::LayoutZone的常量，Quad::acreage换算系数两边工程完全一致，不需要
// 单位换算；换算成一个近似正方形的边长，贴着lot任意一条有路的边居中摆放。这个常量Assign()/
// Layout()都要用到，各自独立算一遍(side本身是纯类型级常量，不需要共享状态)。
namespace {
	constexpr float TARGET_ACREAGE = 20000.f;
}

void ResidenceZone::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	float side = sqrtf(TARGET_ACREAGE / ACREAGE_SCALE_FACTOR);

	for (Lot* lot : lots) {
		if (!lot) continue;

		for (int dir = 0; dir < 4; dir++) {
			if (!lot->GetBoundaryRoad(dir)) continue;
			bool alongY = (dir == FACE_WEST || dir == FACE_EAST);
			float frontage = alongY ? lot->GetSizeY() : lot->GetSizeX();
			if (frontage < side) continue;

			LotPlacementRequest request;
			request.lot = lot;
			request.direction = dir;
			request.marginStart = (frontage - side) / 2.f;
			request.marginEnd = (frontage - side) / 2.f;
			request.depth = side;
			emit(context, request);

			break; // 这块lot只要一个方向，找到第一个可用方向就够了。
		}
	}
}

void ResidenceZone::Layout(int direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	float side = sqrtf(TARGET_ACREAGE / ACREAGE_SCALE_FACTOR);
	float half = side / 2.f;
	int dir = direction;
	bool alongY = (dir == FACE_WEST || dir == FACE_EAST);

	// 围墙/大门/内部道路测试场景(需求8)：dir这条边正中间放一个大门，该边大门以外、以及其余3条
	// 边整段铺围墙；垂直于大门所在边的一条内部道路，从大门位置贯穿到正对面的边，把zone分成
	// 两半——四周都是墙(大门除外)，两个半区应该只通过这条内部路互通。这些字段是ZoneMod级别的
	// (见zone_mod.h)，对这个mod产出的所有zone一视同仁。
	constexpr float WALL_DEPTH = 0.2f;
	constexpr float GATE_WIDTH_RATIO = 0.2f;
	const string wallMesh = "/Game/Asset/Meshes/Fence.Fence";
	// 0.2f照抄老工程Source/Basic/map/zone_basic.cpp里ResidentialZone::LayoutZone对同一个
	// Fence资产设的unit值——之前随手猜了1.f，比栅栏实际长度大5倍，铺出来的实例之间就会留出
	// 一大段空当(BuildWallSegment按这个值换算每段实例的缩放倍数，unit估大了，缩放就跟着偏小)。
	constexpr float wallUnit = 0.2f;

	float gateWidth = side * GATE_WIDTH_RATIO;
	float gateMargin = (side - gateWidth) / 2.f;

	walls.clear();
	gates.clear();
	internalRoads.clear();
	internalBuildings.clear();
	vehicleEntries.clear();
	pedestrianAccess.clear();

	ZoneGateSpec gate;
	gate.direction = dir;
	gate.marginStart = gateMargin;
	gate.marginEnd = gateMargin;
	gate.depth = WALL_DEPTH;
	gates.push_back(gate);

	// 四个方向的进深统一都是WALL_DEPTH，每条边贴墙角的一端缩进也统一都是WALL_DEPTH，
	// 不区分方向——贴大门开口的那一端不缩(大门本身没有厚度，围墙直接顶到开口边缘)。
	ZoneWallSpec wallBeforeGate;
	wallBeforeGate.direction = dir;
	wallBeforeGate.marginStart = WALL_DEPTH; // 贴墙角的一端
	wallBeforeGate.marginEnd = side - gateMargin; // 贴大门开口的一端，不缩
	wallBeforeGate.depth = WALL_DEPTH;
	wallBeforeGate.mesh = wallMesh;
	wallBeforeGate.unit = wallUnit;
	walls.push_back(wallBeforeGate);

	ZoneWallSpec wallAfterGate = wallBeforeGate;
	wallAfterGate.marginStart = side - gateMargin; // 贴大门开口的一端，不缩
	wallAfterGate.marginEnd = WALL_DEPTH; // 贴墙角的一端
	walls.push_back(wallAfterGate);

	// 其余3条边整段铺墙，两端都贴墙角，都缩进WALL_DEPTH。
	for (int otherDir = 0; otherDir < 4; otherDir++) {
		if (otherDir == dir) continue;
		ZoneWallSpec wall;
		wall.direction = otherDir;
		wall.marginStart = WALL_DEPTH;
		wall.marginEnd = WALL_DEPTH;
		wall.depth = WALL_DEPTH;
		wall.mesh = wallMesh;
		wall.unit = wallUnit;
		walls.push_back(wall);
	}

	// 大门局部坐标(原点在zone矩形中心)：dir这条边正中间(marginStart==marginEnd把
	// 大门摆在了正中间，所以沿边方向的坐标就是0)。
	float gateX = (dir == FACE_WEST) ? -half : (dir == FACE_EAST ? half : 0.f);
	float gateY = (dir == FACE_NORTH) ? -half : (dir == FACE_SOUTH ? half : 0.f);

	// 这个测试场景的大门只接人行道(内部道路也只有一条人行道，见下)，不声明车行
	// 出入口——车行入口/出口和人行出入口是完全独立的三个数组，Map::ConnectZoneAccessPoint
	// 按各自的isVehicle参数只断对应类别的车道/人行道，互不影响。
	ZoneAccessPoint access;
	access.x = gateX;
	access.y = gateY;
	access.width = gateWidth;
	pedestrianAccess.push_back(access);

	// 内部道路：垂直于dir这条边，从大门位置贯穿到正对面的边——dir是WEST/EAST时
	// dir边是竖直的，垂直方向是水平线(y取大门的y，也就是0)横贯整个zone；dir是
	// NORTH/SOUTH时反过来，是竖直线(x取大门的x，也就是0)。这条分割线只是一条人行道
	// (isVehicle=false)——按ZoneInternalRoadSpec的约束，一条spec只能是纯车行单行道
	// 或纯人行道二选一，不支持车行+人行混在一条道路里；宽度直接复用上面
	// pedestrianAccess已经指定的gateWidth，不再单独定义一个新的宽度。
	ZoneInternalRoadSpec road;
	if (alongY) {
		road.x1 = -half; road.y1 = 0.f;
		road.x2 = half;  road.y2 = 0.f;
	}
	else {
		road.x1 = 0.f; road.y1 = -half;
		road.x2 = 0.f; road.y2 = half;
	}
	road.isVehicle = false;
	road.width = access.width;
	internalRoads.push_back(road);

	// 内部道路把zone从中间分成两半，每一半各放一个建筑(验证需求8"两个矩形"确实各自
	// 可用)——用和独立建筑同一个类型("building_residence"，Basic层文件间不允许互相
	// include，这里直接写字符串明文而不是引用ResidenceBuilding::GetId())，这样园区内部建筑天然复用
	// ResidenceBuilding::Layout()里的楼体footprint/地下室/楼层数据，和独立建筑在
	// ForeverBuildingFrameworkComponent里走同一套近/远LOD渲染时外观也一致，不需要
	// 给"empty"这个纯占位类型另外补一份长得像的数据。尺寸留出一点余量，不贴着围墙/
	// 内部道路。roadIndices记这个建筑贴着internalRoads[0]的哪一个面——两个半区分别是
	// 内部道路的两侧，各自朝向道路的那个面对应关系相反。direction直接复用各自roadIndices
	// 里已经声明的那个方向(朝向内部道路那一面)。
	float buildingAlong = side * 0.6f;
	float buildingAcross = half * 0.6f;
	ZoneInternalBuildingSpec buildingA, buildingB;
	buildingA.type = buildingB.type = "building_residence";
	buildingA.relativeRotation = buildingB.relativeRotation = 0.f;
	if (alongY) {
		buildingA.x = 0.f; buildingA.y = -half / 2.f;
		buildingA.sizeX = buildingAlong; buildingA.sizeY = buildingAcross;
		buildingA.roadIndices[FACE_SOUTH] = 0;
		buildingA.direction = FACE_SOUTH;

		buildingB.x = 0.f; buildingB.y = half / 2.f;
		buildingB.sizeX = buildingAlong; buildingB.sizeY = buildingAcross;
		buildingB.roadIndices[FACE_NORTH] = 0;
		buildingB.direction = FACE_NORTH;
	}
	else {
		buildingA.x = -half / 2.f; buildingA.y = 0.f;
		buildingA.sizeX = buildingAcross; buildingA.sizeY = buildingAlong;
		buildingA.roadIndices[FACE_EAST] = 0;
		buildingA.direction = FACE_EAST;

		buildingB.x = half / 2.f; buildingB.y = 0.f;
		buildingB.sizeX = buildingAcross; buildingB.sizeY = buildingAlong;
		buildingB.roadIndices[FACE_WEST] = 0;
		buildingB.direction = FACE_WEST;
	}
	internalBuildings.push_back(buildingA);
	internalBuildings.push_back(buildingB);
}
