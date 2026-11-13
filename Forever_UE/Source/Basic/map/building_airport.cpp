#include "building_airport.h"

#include "common/utility.h"

#include <cmath>
#include <algorithm>


using namespace std;

namespace {
	// 把楼体压到靠近道路的一侧：深度方向只占bodyDepthSizeRatio、贴着道路那条边，剩下的深处空间
	// 留给StationMod的站台/跑道，避免公共交通连线穿过楼体。bodyDepthCenterRatio按"贴道路=0、
	// 远离道路=1"给，direction->深度轴的映射和station_common.cpp的NearRoadPoint一致(那边用
	// 单点坐标，这里用区间(centerRatio,sizeRatio)，换算不同所以单独写一份)。
	// bodyFrontageSizeRatio是沿道路方向的占比，不随direction变化。
	BuildingFootprintSpec NearRoadFootprint(int direction, float bodyDepthCenterRatio,
		float bodyDepthSizeRatio, float bodyFrontageSizeRatio) {
		bool depthAlongX = (direction == FACE_WEST || direction == FACE_EAST);
		bool flip = (direction == FACE_EAST || direction == FACE_NORTH);
		float depthCenter = flip ? 1.f - bodyDepthCenterRatio : bodyDepthCenterRatio;

		BuildingFootprintSpec spec;
		if (depthAlongX) {
			spec.centerRatioX = depthCenter; spec.sizeRatioX = bodyDepthSizeRatio;
			spec.centerRatioY = 0.5f; spec.sizeRatioY = bodyFrontageSizeRatio;
		} else {
			spec.centerRatioY = depthCenter; spec.sizeRatioY = bodyDepthSizeRatio;
			spec.centerRatioX = 0.5f; spec.sizeRatioX = bodyFrontageSizeRatio;
		}
		return spec;
	}
	constexpr const char* kComponent = "component_residence";
	constexpr int kComponentId = 0;
}

int AirportBuilding::count = 0;

AirportBuilding::AirportBuilding() : id(count++) {
}

const char* AirportBuilding::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void AirportBuilding::Layout(int& direction, const Quad& quad,
	const std::unordered_map<int, Road*>& boundaryRoads) {
	// 楼体贴路一侧(NearRoadFootprint)，深处留给AirStation::Layout的两条跑道(depthRatio 0.65/0.85)。
	stationMod = "station_air";
	footprint = NearRoadFootprint(direction, 0.18f, 0.35f, 0.6f);
	basements = 0;
	layers = 1;
	floorHeights.assign(1, 0.6f);

	constexpr const char* kComponent = "component_airport";
	constexpr int kComponentId = 0;
	AssignFloor(0, "preset_single_room_fg", direction);
	AssignRoom(0, 0, "room_airport", kComponent, kComponentId);
}

void AirportBuilding::Assign(const vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {
	// 同TrainStationBuilding::Assign的自适应思路：FindAdaptivePlacement会看到先处理的建筑留下的最新
	// freeLots状态(处理顺序由BuildingFactory::GetRegisteredIds()决定，这里不关心)。
	// 占地(沿路x纵深，地图单位)优先12x8(要放跑道和两侧停机位)，放不下退回和火车站一样的7x4。
	constexpr float kFrontages[] = { 12.f, 7.f };
	constexpr float kDepths[] = { 8.f, 4.f };

	for (Lot* lot : lots) {
		if (!lot || lot->GetArea() != AREA_TEST) continue;

		for (int size = 0; size < 2; size++)
		for (int dir = FACE_WEST; dir <= FACE_SOUTH; dir++) {
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
			return;
		}
		return;
	}
}

float AirportBuilding::RandomAcreage() { return 2000.f; }

float AirportBuilding::GetAcreageMin() { return 2000.f; }

float AirportBuilding::GetAcreageMax() { return 2000.f; }

float AirportBuilding::GetPower(AREA_TYPE area) { return 0.f; }
