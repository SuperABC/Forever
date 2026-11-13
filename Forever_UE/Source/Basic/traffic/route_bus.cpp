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

	// speed/dwellSeconds是游戏内秒单位(默认1真实秒=120游戏内秒)，校准依据见route_bus.md。
	speed = 0.025f;
	dwellSeconds = 360.f;

	// 公交每站都停，ta/td会被DriveVehicle钳到单条leg时长的一半，直接给足够大的值即可，见route_bus.md。
	easeSeconds = 20.f;
}

void BusRoute::LayoutRoute(const vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) {
	// 从每个接口自己的departDir反推绕行方向并分两组成环，不假设南北/顺逆时针，见route_bus.md。
	if (interfaces.size() < 4) return; // 凑不出两条至少2站的环线，留空

	float centerX = 0.f, centerY = 0.f;
	for (const RouteStationInfo& info : interfaces) { centerX += info.x; centerY += info.y; }
	centerX /= static_cast<float>(interfaces.size());
	centerY /= static_cast<float>(interfaces.size());

	auto angleAroundCenter = [&](const RouteStationInfo& info) {
		return atan2f(info.y - centerY, info.x - centerX);
	};
	// radial x departDir的2D叉积：>=0的一组按角度升序首尾相接，<0的一组按降序——见下buildLoop。
	auto rotationSign = [&](const RouteStationInfo& info) {
		float rx = info.x - centerX, ry = info.y - centerY;
		return rx * info.departDirY - ry * info.departDirX;
	};

	vector<RouteStationInfo> groupAscending, groupDescending;
	for (const RouteStationInfo& info : interfaces) {
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

		vector<RouteStop> line;
		for (const RouteStationInfo& info : loopStations) {
			line.push_back({ info.stationIndex, info.interfaceIndex });
		}
		lines.push_back(line);

		for (size_t i = 0; i < loopStations.size(); i++) {
			const RouteStationInfo& from = loopStations[i];
			const RouteStationInfo& to = loopStations[(i + 1) % loopStations.size()];
			RouteLink link;
			link.from = { from.stationIndex, from.interfaceIndex };
			link.to = { to.stationIndex, to.interfaceIndex };
			link.bidirectional = false;
			links.push_back(link);
		}
	};
	buildLoop(groupAscending);
	buildLoop(groupDescending);
}
