#include "zone_basic.h"

#include <cmath>

using namespace std;

void ZoneBasic::Distribute(const vector<Lot*>& lots) {
	// 老工程ResidentialZone::LayoutZone的常量，Quad::acreage换算系数两边工程完全一致，
	// 不需要单位换算；换算成一个近似正方形的边长，贴着lot任意一条有路的边居中摆放。
	constexpr float TARGET_ACREAGE = 20000.f;
	float side = sqrtf(TARGET_ACREAGE / ACREAGE_SCALE_FACTOR);
	float half = side / 2.f;

	// 围墙/大门/内部道路测试场景(需求8)：随便一条有真实道路的边正中间放一个大门，该边
	// 大门以外、以及其余3条边整段铺围墙；垂直于大门所在边的一条内部道路，从大门位置贯穿到
	// 正对面的边，把zone分成两半——四周都是墙(大门除外)，两个半区应该只通过这条内部路互通。
	// 这些字段是ZoneMod级别的(见zone_mod.h)，对这个mod产出的所有zone一视同仁；这次用当前
	// 唯一的zone实例测试，按实际选中的方向现算，不写死某个固定direction。
	constexpr float WALL_DEPTH = 0.2f;
	constexpr float GATE_WIDTH_RATIO = 0.2f;
	const string wallMesh = "/Game/Asset/Meshes/Fence.Fence";
	// 0.2f照抄老工程Source/Basic/map/zone_basic.cpp里ResidentialZone::LayoutZone对同一个
	// Fence资产设的unit值——之前随手猜了1.f，比栅栏实际长度大5倍，铺出来的实例之间就会留出
	// 一大段空当(BuildWallSegment按这个值换算每段实例的缩放倍数，unit估大了，缩放就跟着偏小)。
	constexpr float wallUnit = 0.2f;

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
			explicitPlacements.push_back(request);

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
			// 按各自的isVehicle参数只断对应类别的车道/人行道，互不影响；这里如果也
			// push一个vehicleEntries，就会在这个大门位置额外断出一个车行node，
			// 但zone内部根本没有车行路网可接，纯粹是测试数据声明多余，不是接图逻辑本身的问题。
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
			// 可用)——"empty"是Forever_Mod/Empty里的占位building类型，任何配置下都保证已注册，
			// 这里只是验证内部建筑这条链路，不关心具体建筑外观。尺寸留出一点余量，不贴着围墙/
			// 内部道路。roadIndices记这个建筑贴着internalRoads[0]的哪一个面——两个半区分别是
			// 内部道路的两侧，各自朝向道路的那个面对应关系相反。
			float buildingAlong = side * 0.6f;
			float buildingAcross = half * 0.6f;
			ZoneInternalBuildingSpec buildingA, buildingB;
			buildingA.type = buildingB.type = "empty";
			buildingA.relativeRotation = buildingB.relativeRotation = 0.f;
			if (alongY) {
				buildingA.x = 0.f; buildingA.y = -half / 2.f;
				buildingA.sizeX = buildingAlong; buildingA.sizeY = buildingAcross;
				buildingA.roadIndices[FACE_SOUTH] = 0;

				buildingB.x = 0.f; buildingB.y = half / 2.f;
				buildingB.sizeX = buildingAlong; buildingB.sizeY = buildingAcross;
				buildingB.roadIndices[FACE_NORTH] = 0;
			}
			else {
				buildingA.x = -half / 2.f; buildingA.y = 0.f;
				buildingA.sizeX = buildingAcross; buildingA.sizeY = buildingAlong;
				buildingA.roadIndices[FACE_EAST] = 0;

				buildingB.x = half / 2.f; buildingB.y = 0.f;
				buildingB.sizeX = buildingAcross; buildingB.sizeY = buildingAlong;
				buildingB.roadIndices[FACE_WEST] = 0;
			}
			internalBuildings.push_back(buildingA);
			internalBuildings.push_back(buildingB);

			break;
		}
	}
}
