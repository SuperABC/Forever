#include "route_air.h"

#include "route_common.h"

#include <algorithm>
#include <cmath>
#include <cfloat>

using namespace std;

int AirRoute::count = 0;

AirRoute::AirRoute() : id(count++) {
}

const char* AirRoute::GetName() {
	name = "AirRoute" + to_string(id);
	return name.data();
}

void AirRoute::SetProperty() {
	stationType = "plane";
	vehicleType = "vehicle_plane";
	useRoadnet = false;
	instantiate = false;

	// 校准依据见route_air.md：跑一圈约100真实秒、停约6真实秒。
	speed = 0.175f;
	dwellSeconds = 720.f;

	// 飞机比火车更大更重，启动/刹车更缓。
	easeSeconds = 50.f;
}

void AirRoute::LayoutRoute(const vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) {
	constexpr float kCruiseHeight = 100.f; // 飞机巡航高度(地图单位)
	LayoutDualTrackStationLoop(interfaces, sizeX, sizeY, true, kCruiseHeight, edgeStations, links, lines);
}
