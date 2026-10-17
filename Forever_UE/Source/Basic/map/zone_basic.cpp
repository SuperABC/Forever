#include "zone_basic.h"

#include <cmath>


using namespace std;

int ResidenceZone::count = 0;

ResidenceZone::ResidenceZone() : id(count++) {
}

const char* ResidenceZone::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

// 目标面积常量，不需要单位换算；换算成一个近似正方形的边长，贴着lot任意一条有路的边居中
// 摆放。这个常量Assign()/Layout()都要用到，各自独立算一遍(side本身是纯类型级常量，不需要
// 共享状态)。
namespace {
	constexpr float TARGET_ACREAGE = 20000.f;
	// 围墙进深、大门宽度占边长的比例——Layout()铺围墙/大门、LayoutTrees()让出大门都用这两个值。
	constexpr float WALL_DEPTH = 0.2f;
	constexpr float GATE_WIDTH_RATIO = 0.2f;
}

// 园区自定义模型/树的测试配置(先给默认值，看过效果再调)。坐标都按"朝北"(大门在北边、quad
// 局部y=0那条边)写，Core按园区实际大门方向旋转，见feature_spec.h的DecorationSpec/TreeRequests。
// 急救包：大门开口两端各一个，离北边ZONE_KIT_INSET(在0.2厚的栅栏内侧)。
#define ZONE_KIT_MESH_PATH "/Game/3rdParty/Individual_First_Aid_Kit__IFAK_Black_color_/StaticMeshes/individual_first_aid_kit_ifakblack_color.individual_first_aid_kit_ifakblack_color"
#define ZONE_KIT_INSET 0.3f
#define ZONE_KIT_SCALE 1.f
#define ZONE_KIT_YAW 0.f
// 树：沿四面栅栏内侧ZONE_TREE_INSET处每ZONE_TREE_SPACING一棵，大门开口两侧各再让出半个间距。
#define ZONE_TREE_MESH_PATH "/Game/3rdParty/Mossy_Tree/StaticMeshes/mossy-tree.mossy-tree"
#define ZONE_TREE_SCALE 0.6f
#define ZONE_TREE_SPACING 1.f
#define ZONE_TREE_INSET 0.5f

void ResidenceZone::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	float side = sqrtf(TARGET_ACREAGE / ACREAGE_SCALE_FACTOR);

	for (Lot* lot : lots) {
		if (!lot) continue;

		// 井字中心lot(固定AREA_OFFICIAL_HIGH)也允许摆围墙测试场景，和火车站(占北侧)/
		// 机场(占南侧)共存——不排除这个area，只靠下面dir从WEST开始找第一个有边界路、
		// frontage够用的方向这件事本身，自然避开NORTH/SOUTH：这个lot WEST边(中山西路)
		// 肯定有边界路，dir==0(WEST)会第一个命中，不会往后扫到NORTH/SOUTH。真正是否会和
		// 火车站/机场在几何上打架，取决于三边各自占的depth是否真的互不相交，见
		// TrainStationBuilding/AirportBuilding::Assign(depth/margin已经调小，给WEST边的
		// 围墙场景让出空间)。
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
	const string wallMesh = "/Game/Asset/Meshes/Fence.Fence";
	// wallUnit必须匹配Fence资产的实际长度——BuildWallSegment按这个值换算每段实例的缩放
	// 倍数，估大了铺出来的实例之间会留出明显空当。
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
	// 园区大门：滑动双开门，占位先用项目自己的Cube资产，见door_system_plan.md"用户追加的
	// 具体门型要求"一节——双扇对称滑开不需要randomSide(默认false)。
	gate.door.mesh = "/Game/Asset/Meshes/Cube.Cube";
	gate.door.style = 0; // Slide
	gate.door.leaves = 2;
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

	// 内部道路把zone从中间分成两半，每一半各放一个建筑——用和独立建筑同一个类型
	// ("building_residence"，Basic层文件间不允许互相include，这里直接写字符串明文而不是
	// 引用ResidenceBuilding::GetId())，这样园区内部建筑天然复用ResidenceBuilding::Layout()
	// 里的楼体footprint/地下室/楼层数据，和独立建筑在ForeverBuildingFrameworkComponent里
	// 走同一套近/远LOD渲染时外观也一致。尺寸留出一点余量，不贴着围墙/内部道路。roadIndices
	// 记这个建筑贴着internalRoads[0]的哪一个面——两个半区分别是内部道路的两侧，各自朝向
	// 道路的那个面对应关系相反。direction直接复用各自roadIndices里已经声明的那个方向
	// (朝向内部道路那一面)。
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

	// 急救包(测试园区放置mesh)：按朝北坐标写，大门在北边正中间、开口宽GATE_WIDTH_RATIO倍边长，
	// 两个急救包放在开口两端(x=0.5∓开口半宽)、离北边ZONE_KIT_INSET的园区内侧。
	decorations.clear();
	for (float sign : { -1.f, 1.f }) {
		DecorationSpec kit;
		kit.meshPath = ZONE_KIT_MESH_PATH;
		kit.position = { 0.5f + sign * GATE_WIDTH_RATIO * 0.5f, 0.f, 0.f, ZONE_KIT_INSET };
		kit.scaleX = kit.scaleY = kit.scaleZ = ZONE_KIT_SCALE;
		kit.yaw = ZONE_KIT_YAW;
		AddDecoration(kit);
	}
}

void ResidenceZone::LayoutTrees(int direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 按朝北坐标(大门在北边y=0，原点在quad西北角，+X东+Y南)沿四面栅栏内侧种树，Core按大门方向
	// 旋转。位置用PointParams的ratio+offset：ratio=0/1对应西(北)/东(南)边，offset是离边的距离。
	trees = TreeRequests();
	const float side = quad.GetSizeX(); // 园区是正方形(Assign按TARGET_ACREAGE开方取边长)
	const float inset = ZONE_TREE_INSET;
	const float spacing = ZONE_TREE_SPACING;
	auto count = [spacing](float length) { return length > 0.f ? static_cast<int>(length / spacing) + 1 : 0; };
	auto addLine = [&](PointParams from, PointParams to, float length) {
		int num = count(length);
		if (num > 0) AddTreeLine(from, to, num, ZONE_TREE_SCALE, ZONE_TREE_MESH_PATH);
		};

	// 西边、东边：从北角到南角，包含两个角。
	float sideLength = side - 2.f * inset;
	addLine({ 0.f, inset, 0.f, inset }, { 0.f, inset, 1.f, -inset }, sideLength);
	addLine({ 1.f, -inset, 0.f, inset }, { 1.f, -inset, 1.f, -inset }, sideLength);
	// 南边：两个角已经被西/东边种过了，各往里收一个间距。
	addLine({ 0.f, inset + spacing, 1.f, -inset }, { 1.f, -inset - spacing, 1.f, -inset }, sideLength - 2.f * spacing);
	// 北边(大门边)：分成大门两侧两段，大门开口两侧各再让出半个间距(和行道树避让开口同一个规则)，
	// 靠角的一端同样收一个间距避开西/东边已经种过的角。
	float gateClear = GATE_WIDTH_RATIO * 0.5f * side + spacing * 0.5f;
	float northSegment = side * 0.5f - gateClear - inset - spacing;
	addLine({ 0.f, inset + spacing, 0.f, inset }, { 0.5f, -gateClear, 0.f, inset }, northSegment);
	addLine({ 0.5f, gateClear, 0.f, inset }, { 1.f, -inset - spacing, 0.f, inset }, northSegment);
}
