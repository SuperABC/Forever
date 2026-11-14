#include "route_bus.h"

#include <algorithm>
#include <cmath>
#include <cfloat>

using namespace std;

int BusRoute::count = 0;

BusRoute::BusRoute() : id(count++) {
}

const char* BusRoute::GetName() {
	name = "BusRoute" + to_string(id);
	return name.data();
}

void BusRoute::SetProperty() {
	stationType = "bus";
	vehicleType = "vehicle_bus";
	useRoadnet = true;
	instantiate = false;

	// 速度/dwellSeconds是游戏内秒单位(默认1真实秒=120游戏内秒)，校准依据见route_bus.md；公交只用normalSpeed(同站/通过速度用不上)。
	normalSpeed = 0.025f;
	sameStationSpeed = 0.025f;
	passSpeed = 0.025f;
	dwellSeconds = 360.f;

	// 公交每站都停，加减速时间会被Core按单段长度收紧，直接给足够大的值即可，见route_bus.md。
	easeSeconds = 3.f;
}

void BusRoute::LayoutRoute(const vector<RouteStationInfo>& stations, int sizeX, int sizeY) {
	// 公交站只有一个停靠位(下标0)，贴路站点的实际路线由Core用路网车辆寻路图拼，这里只排停靠顺序。
	// 从每个站自己的停靠方向反推绕行方向并分两组成环，不假设南北/顺逆时针，见route_bus.md。
	if (stations.size() < 4) return; // 凑不出两条至少2站的环线，留空

	float centerX = 0.f, centerY = 0.f;
	for (const RouteStationInfo& info : stations) { centerX += info.x; centerY += info.y; }
	centerX /= static_cast<float>(stations.size());
	centerY /= static_cast<float>(stations.size());

	auto angleAroundCenter = [&](const RouteStationInfo& info) {
		return atan2f(info.y - centerY, info.x - centerX);
	};
	// radial x 行驶方向的2D叉积：>=0的一组按角度升序首尾相接，<0的一组按降序——见下buildLoop。
	auto rotationSign = [&](const RouteStationInfo& info) {
		float rx = info.x - centerX, ry = info.y - centerY;
		const RoutePort& stop = info.stops[0];
		return rx * stop.dirY - ry * stop.dirX;
	};

	vector<RouteStationInfo> groupAscending, groupDescending;
	for (const RouteStationInfo& info : stations) {
		if (info.stops.empty()) continue;
		(rotationSign(info) >= 0.f ? groupAscending : groupDescending).push_back(info);
	}
	sort(groupAscending.begin(), groupAscending.end(), [&](const RouteStationInfo& a, const RouteStationInfo& b) {
		return angleAroundCenter(a) < angleAroundCenter(b);
	});
	sort(groupDescending.begin(), groupDescending.end(), [&](const RouteStationInfo& a, const RouteStationInfo& b) {
		return angleAroundCenter(a) > angleAroundCenter(b);
	});

	auto buildLoop = [&](const vector<RouteStationInfo>& loopStations) {
		if (loopStations.size() < 2) return; // 单站凑不成环，丢弃
		vector<RouteLineEntry> line;
		for (const RouteStationInfo& info : loopStations) line.push_back({ info.name, 0 });
		lines.push_back(std::move(line));
	};
	buildLoop(groupAscending);
	buildLoop(groupDescending);
}
