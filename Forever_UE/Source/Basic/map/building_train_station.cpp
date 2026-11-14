#include "building_train_station.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>


using namespace std;

namespace {
	constexpr const char* kComponent = "component_residence";
	constexpr int kComponentId = 0;
}

int TrainStationBuilding::count = 0;

TrainStationBuilding::TrainStationBuilding() : id(count++) {
}

const char* TrainStationBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void TrainStationBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 楼体占满整个Building区域(不像机场只占贴路一侧)，TrainStation::Layout的两条轨道/四个接口
	// (depthRatio 0.45/0.55)因此落在楼体范围之内，见station_*.md。
	stationMod = "station_train";
	footprint = BuildingFootprintSpec();   // 默认就是中心(0.5,0.5)、占满(1,1)
	basements = 0;
	layers = 3;
	floorHeights = { 0.75f, 0.75f, 1.0f };   // 1楼0.75、2楼0.75、3楼1.0，没有地下层

	constexpr const char* kComponent = "component_train_station";
	constexpr int kComponentId = 0;
	FloorAssetSpec assets;
	assets.stairMeshPath = "/Game/Asset/Building/Mesh/Stair_Line.Stair_Line";
	// 1楼大厅fg+(4间空房)+2楼fl+-+3楼fu-，三层各有4台电梯，没有地下层
	AssignFloor(0, "preset_station_train_fg+", direction, assets);
	AssignFloor(1, "preset_station_train_fl+-", direction, assets);
	AssignFloor(2, "preset_station_train_fu-", direction, assets);
	AssignRoom(0, 0, "room_empty", kComponent, kComponentId);
	AssignRoom(0, 1, "room_empty", kComponent, kComponentId);
	AssignRoom(0, 2, "room_empty", kComponent, kComponentId);
	AssignRoom(0, 3, "room_empty", kComponent, kComponentId);
	AssignElevatorCabin(0, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	AssignElevatorCabin(1, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	AssignElevatorCabin(2, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
	AssignElevatorCabin(3, -basements, layers - 1, "", "script_empty", { "elevator_basic" });
}

void TrainStationBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 一个中央车站(随机一块高密度商业区) + 东南西北各一座(中/低密度商业区)，规则见building_train_station.md。
	constexpr float kMinEdge = 15.f;        // 沿路边长小于它拒绝
	constexpr float kFullEdgeMax = 20.f;    // 沿路边长小于它占满，否则只取一个角
	constexpr float kCornerFrontage = 15.f; // 取角时的沿路长度
	constexpr float kMinLotDepth = 5.f;     // 进深小于它拒绝
	constexpr float kFullDepthMax = 10.f;   // 进深不超过它占满，超过只占kBigDepth
	constexpr float kBigDepth = 7.5f;
	constexpr float kMinCentralDistance = 200.f; // 外围车站所在地块到中央车站所在地块的最小距离
	constexpr float kCos45 = 0.70710678f;

	struct Option { int direction; float frontage; float depth; };
	// 车站沿路方向(世界坐标单位向量，按地块旋转换算)：车站的轨道就沿它走。
	auto alongOf = [](Lot* lot, int dir, float& ax, float& ay) {
		bool alongLocalX = (dir == FACE_NORTH || dir == FACE_SOUTH);
		float rotation = lot->GetRotation();
		ax = alongLocalX ? cosf(rotation) : -sinf(rotation);
		ay = alongLocalX ? sinf(rotation) : cosf(rotation);
	};
	// 这块地能放车站的(朝向，沿路边长，进深)选项：朝向必须贴着路。
	auto optionsOf = [&](Lot* lot) {
		vector<Option> result;
		for (int dir = FACE_WEST; dir <= FACE_SOUTH; dir++) {
			if (!lot->GetBoundaryRoad(dir)) continue;
			bool alongLocalX = (dir == FACE_NORTH || dir == FACE_SOUTH);
			float frontage = alongLocalX ? lot->GetSizeX() : lot->GetSizeY();
			float depth = alongLocalX ? lot->GetSizeY() : lot->GetSizeX();
			if (frontage < kMinEdge || depth < kMinLotDepth) continue;
			result.push_back({ dir, frontage, depth });
		}
		return result;
	};
	auto emitOption = [&](Lot* lot, const Option& option) {
		LotPlacementRequest request;
		request.lot = lot;
		request.direction = option.direction;
		// 取角贴沿路方向的末端(机场贴起端)，同一块地两者共存时不重叠。
		request.marginStart = option.frontage < kFullEdgeMax ? 0.f : option.frontage - kCornerFrontage;
		request.marginEnd = 0.f;
		request.depth = option.depth <= kFullDepthMax ? option.depth : kBigDepth;
		emit(context, request);
	};

	// 中央车站：随机一块能放的高密度商业区。
	vector<Lot*> centralCandidates;
	for (Lot* lot : lots) {
		if (lot && lot->GetArea() == AREA_COMMERCE_HIGH && !optionsOf(lot).empty()) centralCandidates.push_back(lot);
	}
	if (centralCandidates.empty()) {
		// 中央车站放不了：退化成在随机一块中/低密度商业区放一座车站(朝向不限)，route里它两端直线连到地图外。
		vector<Lot*> fallbackCandidates;
		for (Lot* lot : lots) {
			if (lot && (lot->GetArea() == AREA_COMMERCE_MIDDLE || lot->GetArea() == AREA_COMMERCE_LOW) && !optionsOf(lot).empty()) fallbackCandidates.push_back(lot);
		}
		if (fallbackCandidates.empty()) return;
		Lot* lot = fallbackCandidates[GetRandom(static_cast<int>(fallbackCandidates.size()))];
		vector<Option> lotOptions = optionsOf(lot);
		emitOption(lot, lotOptions[GetRandom(static_cast<int>(lotOptions.size()))]);
		return;
	}
	Lot* central = centralCandidates[GetRandom(static_cast<int>(centralCandidates.size()))];
	vector<Option> centralOptions = optionsOf(central);
	const Option& centralOption = centralOptions[GetRandom(static_cast<int>(centralOptions.size()))];
	emitOption(central, centralOption);
	// 中央车站的走向(世界坐标)：外围车站必须落在它某一端的正前方，不然轨道从中央出来再拐去外围站会是钝角。
	float centralAx, centralAy;
	alongOf(central, centralOption.direction, centralAx, centralAy);

	// 东南西北各一座：地块中心到中央地块中心的距离>=200，连线和该方向轴线夹角<=45度(x比中央大就是东，y比中央大就是南)，
	// 连线和中央车站走向夹角<=45度(落在中央车站某一端的正前方)；选边时东西站取沿路方向更贴近东西的边、南北站取更贴近南北的边。
	const float directionX[4] = { 1.f, 0.f, -1.f, 0.f };  // 东、南、西、北
	const float directionY[4] = { 0.f, 1.f, 0.f, -1.f };
	for (int d = 0; d < 4; d++) {
		struct Candidate { Lot* lot; Option option; };
		vector<Candidate> candidates;
		for (Lot* lot : lots) {
			if (!lot || (lot->GetArea() != AREA_COMMERCE_MIDDLE && lot->GetArea() != AREA_COMMERCE_LOW)) continue;
			float dx = lot->GetPosX() - central->GetPosX(), dy = lot->GetPosY() - central->GetPosY();
			float distance = sqrtf(dx * dx + dy * dy);
			if (distance < kMinCentralDistance) continue;
			float ux = dx / distance, uy = dy / distance;
			if (ux * directionX[d] + uy * directionY[d] < kCos45 - 1e-4f) continue;
			// 连线和中央车站走向的夹角<=45度(走向是无向轴，取靠近这座站的那一端)：西南-东北走向的话，西站只能在西南端前方。
			if (fabsf(ux * centralAx + uy * centralAy) < kCos45 - 1e-4f) continue;
			// 选边：东/西站取沿路方向更贴近东西走向的那条边，南/北站取更贴近南北走向的(directionX/Y就是该方向的轴)。
			const Option* best = nullptr;
			float bestAlign = -1.f;
			vector<Option> lotOptions = optionsOf(lot);
			for (const Option& option : lotOptions) {
				float ax, ay;
				alongOf(lot, option.direction, ax, ay);
				float align = fabsf(ax * fabsf(directionX[d]) + ay * fabsf(directionY[d]));
				if (align > bestAlign) { bestAlign = align; best = &option; }
			}
			if (best) candidates.push_back({ lot, *best });
		}
		if (candidates.empty()) continue;
		const Candidate& pick = candidates[GetRandom(static_cast<int>(candidates.size()))];
		emitOption(pick.lot, pick.option);
	}
}

float TrainStationBuilding::RandomAcreage() { return 2000.f; }

float TrainStationBuilding::GetAcreageMin() { return 2000.f; }

float TrainStationBuilding::GetAcreageMax() { return 2000.f; }

float TrainStationBuilding::GetPower(AREA_TYPE area) { return 0.f; }
