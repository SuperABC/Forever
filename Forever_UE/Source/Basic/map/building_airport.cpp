#include "building_airport.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>


using namespace std;

int AirportBuilding::count = 0;

AirportBuilding::AirportBuilding() : id(count++) {
}

const char* AirportBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void AirportBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 楼体占满整个Building区域(默认footprint，同火车站)，AirStation::Layout的两条跑道(depthRatio 0.7/0.9)因此落在楼体范围之内。
	stationMod = "station_air";
	footprint = BuildingFootprintSpec();   // 默认就是中心(0.5,0.5)、占满(1,1)
	basements = 0;
	layers = 2;
	floorHeights = { 0.6f, 0.6f };

	constexpr const char* kComponent = "component_airport";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.stairMeshPath = "/Game/Asset/Building/Mesh/Stair_Line.Stair_Line";
	// 1楼大厅airport_fg+(10间房+2个停机坪+2台电梯井+楼梯)，2楼airport_fu-(2间房+2台电梯井)。
	AssignFloor(0, "preset_airport_fg+", direction, assets);
	AssignFloor(1, "preset_airport_fu-", direction, assets);
	// 房间先全部是空房(1楼10个single槽位、2楼2个)；机场的业务房间以后再换。
	for (int slot = 0; slot < 10; slot++) AssignRoom(0, slot, "room_empty", kComponent, kComponentId);
	for (int slot = 0; slot < 2; slot++) AssignRoom(1, slot, "room_empty", kComponent, kComponentId);
	// 1楼2个停机坪(layout的parkings 0~1，各自和single 0~1同一个矩形)：spacing=-1=整个矩形就是一个机位，
	// 所以矩形多宽都只会有一架飞机(机场占地随地块变，不能靠固定间距凑)。机位归入矩形对应的空房。
	// stocked：开局每个机位停一架飞机(飞机没有主人，不走AssignVehicles)。
	for (int i = 0; i < 2; i++) AssignStockedParking(0, i, i, -1.f, { "plane" });
	// 两层各2台电梯，3台电梯井编号同layout的elevators。
	AssignElevatorCabin(0, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	AssignElevatorCabin(1, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
}

void AirportBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 已放机场的中心点，低密度商业区之间要互相离开kMinAirportDistance才再放一座。
	vector<pair<float, float>> airports;

	// jing路网的AREA_TEST中心地块固定放一座：FindAdaptivePlacement会看到先处理的建筑留下的最新freeLots状态
	// (处理顺序由BuildingFactory::GetRegisteredIds()决定，这里不关心)。占地(沿路x纵深)优先12x8，放不下退回7x4。
	constexpr float kFrontages[] = { 12.f, 7.f };
	constexpr float kDepths[] = { 8.f, 4.f };
	for (Lot* lot : lots) {
		if (!lot || lot->GetArea() != AREA_TEST) continue;

		bool placed = false;
		for (int size = 0; size < 2 && !placed; size++)
		for (int dir = FACE_WEST; dir <= FACE_SOUTH && !placed; dir++) {
			Lot* targetLot;
			float marginStart, marginEnd, depth;
			if (!lot->FindAdaptivePlacement(dir, kFrontages[size], kDepths[size], targetLot, marginStart, marginEnd, depth)) continue;

			// 余量全挪到marginStart，贴另一端的角(和火车站相反)，两者同在一条边时占两个对角。
			marginStart += marginEnd;
			marginEnd = 0.f;

			LotPlacementRequest request;
			request.lot = targetLot;
			request.adaptiveParent = (targetLot != lot) ? lot : nullptr;
			request.direction = dir;
			request.marginStart = marginStart;
			request.marginEnd = marginEnd;
			request.depth = depth;
			emit(context, request);
			airports.push_back({ lot->GetPosX(), lot->GetPosY() });
			placed = true;
		}
		break;
	}

	// 低密度商业区：取最长边为沿路方向，规则见building_airport.md。
	constexpr float kMinEdge = 20.f;        // 最长边小于它跳过
	constexpr float kFullEdgeMax = 40.f;    // 最长边小于它沿路占满，否则取一个角
	constexpr float kCornerFrontage = 30.f; // 取角时的沿路长度
	constexpr float kMinDepth = 10.f;       // 进深小于它跳过
	constexpr float kFullDepthMax = 20.f;   // 进深不超过它占满，超过只占kBigDepth
	constexpr float kBigDepth = 15.f;
	constexpr float kMinAirportDistance = 800.f;
	for (Lot* lot : lots) {
		if (!lot || lot->GetArea() != AREA_COMMERCE_LOW) continue;

		bool alongX = lot->GetSizeX() >= lot->GetSizeY();
		float edge = alongX ? lot->GetSizeX() : lot->GetSizeY();
		float lotDepth = alongX ? lot->GetSizeY() : lot->GetSizeX();
		if (edge < kMinEdge || lotDepth < kMinDepth) continue;

		bool tooClose = false;
		for (const auto& airport : airports) {
			float dx = lot->GetPosX() - airport.first, dy = lot->GetPosY() - airport.second;
			if (dx * dx + dy * dy <= kMinAirportDistance * kMinAirportDistance) { tooClose = true; break; }
		}
		if (tooClose) continue;

		// 沿路方向的两侧里取有边界道路的一侧。
		int dirs[2] = { alongX ? FACE_NORTH : FACE_WEST, alongX ? FACE_SOUTH : FACE_EAST };
		int direction = -1;
		for (int dir : dirs) {
			if (lot->GetBoundaryRoad(dir)) { direction = dir; break; }
		}
		if (direction < 0) continue;

		LotPlacementRequest request;
		request.lot = lot;
		request.direction = direction;
		request.marginStart = 0.f;
		request.marginEnd = edge < kFullEdgeMax ? 0.f : edge - kCornerFrontage;
		request.depth = lotDepth <= kFullDepthMax ? lotDepth : kBigDepth;
		emit(context, request);
		airports.push_back({ lot->GetPosX(), lot->GetPosY() });
	}
}

float AirportBuilding::RandomAcreage() { return 2000.f; }

float AirportBuilding::GetAcreageMin() { return 2000.f; }

float AirportBuilding::GetAcreageMax() { return 2000.f; }

float AirportBuilding::GetPower(AREA_TYPE area) { return 0.f; }
