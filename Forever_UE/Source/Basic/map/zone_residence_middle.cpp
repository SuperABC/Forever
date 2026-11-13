#include "zone_residence_middle.h"

#include "common/utility.h"

#include <cmath>


using namespace std;

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

int ResidenceMiddleZone::count = 0;

ResidenceMiddleZone::ResidenceMiddleZone() : id(count++) {
}

const char* ResidenceMiddleZone::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void ResidenceMiddleZone::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 目标面积换算成近似正方形的边长；沿路宽度和进深是最小值，切完某一侧剩下的地不到3个单位时园区会被
	// 拉伸到那条边(见general_geometry.md的RequestPlacement)，Layout()按实际quad尺寸排布。
	constexpr float TARGET_ACREAGE = 20000.f;
	const float side = sqrtf(TARGET_ACREAGE / ACREAGE_SCALE_FACTOR);

	for (Lot* lot : lots) {
		if (!lot || lot->GetArea() != AREA_RESIDENCE_MIDDLE) continue;

		// 每块middle密度住宅区lot里随机放0/1/2个middle密度住宅园区。
		const int num = GetRandom(3);
		if (num == 0) continue;

		// 找第一个有边界路、临街长度够用的方向；第1个园区贴这条边起点那个角(不居中)。
		for (int dir = 0; dir < 4; dir++) {
			if (!lot->GetBoundaryRoad(dir)) continue;
			bool alongY = (dir == FACE_WEST || dir == FACE_EAST);
			float frontage = alongY ? lot->GetSizeY() : lot->GetSizeX();
			if (frontage < side) continue;

			LotPlacementRequest request;
			request.lot = lot;
			request.direction = dir;
			request.marginStart = 0.f;
			request.marginEnd = frontage - side;
			request.depth = side;
			emit(context, request);

			if (num >= 2) {
				// 第2个园区：这条边够长就贴另一端的角，不够长就换到正对的那条边(WEST<->EAST、NORTH<->SOUTH)
				// 的起点角；放不下时RequestPlacement会失败、这个请求被跳过，不影响第1个。
				LotPlacementRequest second = request;
				if (frontage >= 2.f * side) {
					second.marginStart = frontage - side;
					second.marginEnd = 0.f;
					emit(context, second);
				}
				else {
					int opposite = dir ^ 1;
					if (lot->GetBoundaryRoad(opposite)) {
						float oppositeFrontage = (opposite == FACE_WEST || opposite == FACE_EAST) ? lot->GetSizeY() : lot->GetSizeX();
						if (oppositeFrontage >= side) {
							second.direction = opposite;
							second.marginStart = 0.f;
							second.marginEnd = oppositeFrontage - side;
							emit(context, second);
						}
					}
				}
			}
			break;
		}
	}
}

void ResidenceMiddleZone::Layout(int direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 挂示例脚本entity_event(进出事件只发给这个实体自己的Script)，见Resource/Story/entity_event.script。
	milestoneNames = { "entity_event" };
	// 园区地面铺人行道材质。
	gapMaterial = "/Game/Asset/Building/Material/Pavement.Pavement";
	// 目标面积换算成近似正方形的边长；围墙进深、大门宽度占边长的比例。
	constexpr float TARGET_ACREAGE = 20000.f;
	constexpr float WALL_DEPTH = 0.2f;
	constexpr float GATE_WIDTH_RATIO = 0.2f;
	// 园区实际尺寸以quad为准(占位时尽量是TARGET_ACREAGE开方的正方形，但某一侧空隙不够切时会被拉伸)。
	const float halfX = quad.GetSizeX() / 2.f, halfY = quad.GetSizeY() / 2.f;
	int dir = direction;
	bool alongY = (dir == FACE_WEST || dir == FACE_EAST);
	const float side = alongY ? quad.GetSizeY() : quad.GetSizeX(); // 大门所在那条边的长度

	// 围墙/大门/内部道路字段是ZoneMod级别的(见zone_mod.h)，布局见zone_residence_middle.md。
	const string wallMesh = "/Game/Asset/Zone/Default/Mesh/Fence.Fence";
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
	// 园区大门：骨骼动画门(目前所有门共用测试门)，车行门很宽，会被拉得很大。
	gate.door.mesh = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/SKM_door_with_cardreader.SKM_door_with_cardreader";
	gate.door.animClass = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/ABP_Door.ABP_Door_C";
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
	float gateX = (dir == FACE_WEST) ? -halfX : (dir == FACE_EAST ? halfX : 0.f);
	float gateY = (dir == FACE_NORTH) ? -halfY : (dir == FACE_SOUTH ? halfY : 0.f);

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
		road.x1 = -halfX; road.y1 = 0.f;
		road.x2 = halfX;  road.y2 = 0.f;
	}
	else {
		road.x1 = 0.f; road.y1 = -halfY;
		road.x2 = 0.f; road.y2 = halfY;
	}
	road.isVehicle = false;
	road.width = access.width;
	internalRoads.push_back(road);

	// 内部道路把园区分成两半，每半一栋building_residence_middle(Basic层文件间不能互相include，所以写
	// 字符串明文)。尺寸留余量不贴围墙/内部道路；roadIndices记贴着internalRoads[0]的哪个面，
	// 两半相反，direction取该面朝向。
	// 沿内部道路方向取园区在这个方向的长度，垂直方向取半边长度。
	float buildingAlong = (alongY ? quad.GetSizeX() : quad.GetSizeY()) * 0.6f;
	float buildingAcross = (alongY ? halfY : halfX) * 0.6f;
	ZoneInternalBuildingSpec buildingA, buildingB;
	buildingA.type = buildingB.type = "building_residence_middle";
	buildingA.relativeRotation = buildingB.relativeRotation = 10.f * 3.14159265f / 180.f; // 测试：两栋楼相对园区旋转10度(弧度)
	if (alongY) {
		buildingA.x = 0.f; buildingA.y = -halfY / 2.f;
		buildingA.sizeX = buildingAlong; buildingA.sizeY = buildingAcross;
		buildingA.roadIndices[FACE_SOUTH] = 0;
		buildingA.direction = FACE_SOUTH;

		buildingB.x = 0.f; buildingB.y = halfY / 2.f;
		buildingB.sizeX = buildingAlong; buildingB.sizeY = buildingAcross;
		buildingB.roadIndices[FACE_NORTH] = 0;
		buildingB.direction = FACE_NORTH;
	}
	else {
		buildingA.x = -halfX / 2.f; buildingA.y = 0.f;
		buildingA.sizeX = buildingAcross; buildingA.sizeY = buildingAlong;
		buildingA.roadIndices[FACE_EAST] = 0;
		buildingA.direction = FACE_EAST;

		buildingB.x = halfX / 2.f; buildingB.y = 0.f;
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

void ResidenceMiddleZone::LayoutTrees(int direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 按朝北坐标(大门在北边y=0，原点在quad西北角，+X东+Y南)沿四面栅栏内侧种树，Core按大门方向
	// 旋转。位置用PointParams的ratio+offset：ratio=0/1对应西(北)/东(南)边，offset是离边的距离。
	constexpr float GATE_WIDTH_RATIO = 0.2f; // 同Layout()的大门宽度占边长的比例
	trees = TreeRequests();
	// 朝北坐标系里大门所在的边(北边)长度side、南北方向深度depth：大门朝西/东时和quad的X/Y互换。
	const bool gateAlongY = (direction == FACE_WEST || direction == FACE_EAST);
	const float side = gateAlongY ? quad.GetSizeY() : quad.GetSizeX();
	const float depth = gateAlongY ? quad.GetSizeX() : quad.GetSizeY();
	const float inset = ZONE_TREE_INSET;
	const float spacing = ZONE_TREE_SPACING;
	auto count = [spacing](float length) { return length > 0.f ? static_cast<int>(length / spacing) + 1 : 0; };
	auto addLine = [&](PointParams from, PointParams to, float length) {
		int num = count(length);
		if (num > 0) AddTreeLine(from, to, num, ZONE_TREE_SCALE, ZONE_TREE_MESH_PATH);
		};

	// 西边、东边：从北角到南角(长度是深度方向)，包含两个角。
	float sideLength = depth - 2.f * inset;
	addLine({ 0.f, inset, 0.f, inset }, { 0.f, inset, 1.f, -inset }, sideLength);
	addLine({ 1.f, -inset, 0.f, inset }, { 1.f, -inset, 1.f, -inset }, sideLength);
	// 南边：两个角已经被西/东边种过了，各往里收一个间距。
	addLine({ 0.f, inset + spacing, 1.f, -inset }, { 1.f, -inset - spacing, 1.f, -inset }, side - 2.f * inset - 2.f * spacing);
	// 北边(大门边)：分成大门两侧两段，大门开口两侧各再让出半个间距(和行道树避让开口同一个规则)，
	// 靠角的一端同样收一个间距避开西/东边已经种过的角。
	float gateClear = GATE_WIDTH_RATIO * 0.5f * side + spacing * 0.5f;
	float northSegment = side * 0.5f - gateClear - inset - spacing;
	addLine({ 0.f, inset + spacing, 0.f, inset }, { 0.5f, -gateClear, 0.f, inset }, northSegment);
	addLine({ 0.5f, gateClear, 0.f, inset }, { 1.f, -inset - spacing, 0.f, inset }, northSegment);
}
