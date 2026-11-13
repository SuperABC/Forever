#include "station_bus.h"

#include "map/general_geometry.h"

using namespace std;

int BusStation::count = 0;

BusStation::BusStation() : id(count++) {
}

const char* BusStation::GetName() {
	name = "BusStation" + to_string(id);
	return name.data();
}

void BusStation::SetProperty() {
	stationType = "bus";
}

void BusStation::AssignRoads(const vector<Road*>& roads, RoadStationEmitFunc emit, void* context) {
	// 只贴井字路网中心正方形的四条边道路(JingRoadnet::DistributeRoadnet里固定的名字)，
	// 每条路取中点(t=0.5)，两侧各摆一个。
	static const char* kCenterRoadNames[] = { "中山西路", "中山东路", "中山北路", "中山南路" };

	for (Road* road : roads) {
		if (!road) continue;

		bool isCenterRoad = false;
		for (const char* roadName : kCenterRoadNames) {
			if (road->GetName() == roadName) { isCenterRoad = true; break; }
		}
		if (!isCenterRoad) continue;

		RoadStationRequest left{ road, 0.5f, false };
		RoadStationRequest right{ road, 0.5f, true };
		emit(context, left);
		emit(context, right);
	}
}
