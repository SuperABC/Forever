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

// 目标面积换算成近似正方形的边长side；Assign()/Layout()各自独立算一遍(纯类型级常量，不需要共享状态)。
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

		// 井字中心lot(AREA_OFFICIAL_HIGH)也允许摆园区：dir从WEST开始扫，中心lot的WEST边
		// 必有边界路，会第一个命中，园区只占这条边起点角的一块，剩余空间留给火车站/机场。
		for (int dir = 0; dir < 4; dir++) {
			if (!lot->GetBoundaryRoad(dir)) continue;
			bool alongY = (dir == FACE_WEST || dir == FACE_EAST);
			float frontage = alongY ? lot->GetSizeY() : lot->GetSizeX();
			if (frontage < side) continue;

			LotPlacementRequest request;
			request.lot = lot;
			request.direction = dir;
			// 贴到这条边起点那个角，不居中：把完整的大块空地留给后面的火车站/机场/Yuanshen。
			request.marginStart = 0.f;
			request.marginEnd = frontage - side;
			request.depth = side;
			emit(context, request);

			break; // 这块lot只要一个方向，找到第一个可用方向就够了。
		}
	}
}

void ResidenceZone::Layout(int direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 挂示例脚本entity_event(进出事件只发给这个实体自己的Script)，见Resource/Story/entity_event.script。
	milestoneNames = { "entity_event" };
	float side = sqrtf(TARGET_ACREAGE / ACREAGE_SCALE_FACTOR);
	float half = side / 2.f;
	int dir = direction;
	bool alongY = (dir == FACE_WEST || dir == FACE_EAST);

	// 围墙/大门/内部道路字段是ZoneMod级别的(见zone_mod.h)，布局见zone_basic.md。
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

	// 内部道路：垂直于dir这条边、过大门贯穿到正对面的边。ZoneInternalRoadSpec一条spec只能是纯
	// 车行单行道或纯人行道二选一，这里是人行道(isVehicle=false)，宽度复用大门宽度。
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

	// 内部道路把园区分成两半，每半一栋building_residence(Basic层文件间不能互相include，所以写
	// 字符串明文)。尺寸留余量不贴围墙/内部道路；roadIndices记贴着internalRoads[0]的哪个面，
	// 两半相反，direction取该面朝向。
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
