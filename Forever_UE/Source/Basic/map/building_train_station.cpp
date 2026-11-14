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
	// 这块地能放车站的(朝向，沿路边长，进深)选项：朝向必须贴着路。constrained时车站沿路方向(世界坐标)和(ux,uy)的夹角要<=45度。
	auto optionsOf = [&](Lot* lot, bool constrained, float ux, float uy) {
		vector<Option> result;
		for (int dir = FACE_WEST; dir <= FACE_SOUTH; dir++) {
			if (!lot->GetBoundaryRoad(dir)) continue;
			bool alongLocalX = (dir == FACE_NORTH || dir == FACE_SOUTH);
			float frontage = alongLocalX ? lot->GetSizeX() : lot->GetSizeY();
			float depth = alongLocalX ? lot->GetSizeY() : lot->GetSizeX();
			if (frontage < kMinEdge || depth < kMinLotDepth) continue;
			if (constrained) {
				float rotation = lot->GetRotation();
				float ax = alongLocalX ? cosf(rotation) : -sinf(rotation);
				float ay = alongLocalX ? sinf(rotation) : cosf(rotation);
				if (fabsf(ax * ux + ay * uy) < kCos45 - 1e-4f) continue;
			}
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
		if (lot && lot->GetArea() == AREA_COMMERCE_HIGH && !optionsOf(lot, false, 0.f, 0.f).empty()) centralCandidates.push_back(lot);
	}
	if (centralCandidates.empty()) {
		// 中央车站放不了：退化成在随机一块中/低密度商业区放一座车站(朝向不限)，route里它两端直线连到地图外。
		vector<Lot*> fallbackCandidates;
		for (Lot* lot : lots) {
			if (lot && (lot->GetArea() == AREA_COMMERCE_MIDDLE || lot->GetArea() == AREA_COMMERCE_LOW) && !optionsOf(lot, false, 0.f, 0.f).empty()) fallbackCandidates.push_back(lot);
		}
		if (fallbackCandidates.empty()) return;
		Lot* lot = fallbackCandidates[GetRandom(static_cast<int>(fallbackCandidates.size()))];
		vector<Option> lotOptions = optionsOf(lot, false, 0.f, 0.f);
		emitOption(lot, lotOptions[GetRandom(static_cast<int>(lotOptions.size()))]);
		return;
	}
	Lot* central = centralCandidates[GetRandom(static_cast<int>(centralCandidates.size()))];
	vector<Option> centralOptions = optionsOf(central, false, 0.f, 0.f);
	emitOption(central, centralOptions[GetRandom(static_cast<int>(centralOptions.size()))]);

	// 东南西北各一座：地块中心到中央地块中心的距离>=200，连线和该方向轴线夹角<=45度(x比中央大就是东，y比中央大就是南)，
	// 车站沿路方向和连线夹角<=45度。
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
			for (const Option& option : optionsOf(lot, true, ux, uy)) candidates.push_back({ lot, option });
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
