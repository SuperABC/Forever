#include "route_bus.h"

#include "bus_stop_name.h"
#include "common/mod_args.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <map>

using namespace std;

int BusRoute::count = 0;

BusRoute::BusRoute(const string& args) : id(count++) {
	skipAuto = ModArgs(args).GetBool("skip");
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
	// 同名道路(棋盘格里同一条横/竖线)上的站连成一条线：去程站沿路升序、回程站降序首尾相接成环，
	// 两端尽头靠Core寻路经路口的U形连接掉头回到对侧站，见route_bus.md。
	constexpr int kStationsPerVehicle = 4; // 估值：每这么多站配一辆车
	if (skipAuto) { LayoutManual(); return; }

	map<string, vector<const RouteStationInfo*>> byRoad;
	for (const RouteStationInfo& info : stations) {
		if (info.roadName.empty() || info.stops.empty()) continue;
		byRoad[info.roadName].push_back(&info);
	}

	for (auto& [roadName, group] : byRoad) {
		// 参考方向取第一个站的行驶方向；同一条线上的路近似共线(棋盘格旋转不超过约0.2弧度)，用它做沿路坐标。
		float refX = group[0]->stops[0].dirX, refY = group[0]->stops[0].dirY;
		auto along = [&](const RouteStationInfo* info) { return info->x * refX + info->y * refY; };

		vector<const RouteStationInfo*> forward, backward;
		for (const RouteStationInfo* info : group) {
			const RoutePort& stop = info->stops[0];
			(stop.dirX * refX + stop.dirY * refY >= 0.f ? forward : backward).push_back(info);
		}
		if (forward.empty() || backward.empty()) continue; // 没有对侧站，掉不了头
		sort(forward.begin(), forward.end(), [&](const RouteStationInfo* a, const RouteStationInfo* b) { return along(a) < along(b); });
		sort(backward.begin(), backward.end(), [&](const RouteStationInfo* a, const RouteStationInfo* b) { return along(a) > along(b); });

		vector<RouteLineEntry> line;
		for (const RouteStationInfo* info : forward) line.push_back({ info->name, 0 });
		for (const RouteStationInfo* info : backward) line.push_back({ info->name, 0 });
		lineVehicleCounts.push_back(max(1, static_cast<int>(line.size()) / kStationsPerVehicle));
		lines.push_back(std::move(line));
	}
}

void BusRoute::LayoutManual() {
	// 手动线路(--skip)：编辑器"路网路线"的每条线是一个环，站依次是BusStopName(线下标,站下标)，首尾相接由Core补；
	// 站由station_bus::AssignManualRoads按同名建出来，缺站/不通的线由Core Assemble丢掉。
	constexpr int kStationsPerVehicle = 4;
	if (!manual) {
		debugf("Warning: BusRoute --skip but no manual transit data is available, no bus lines.\n");
		return;
	}
	for (size_t lineIndex = 0; lineIndex < manual->roadLines.size(); lineIndex++) {
		const TransitData::RoadLine& roadLine = manual->roadLines[lineIndex];
		if (roadLine.route != GetType() || roadLine.stops.size() < 2) continue;
		vector<RouteLineEntry> line;
		for (size_t stopIndex = 0; stopIndex < roadLine.stops.size(); stopIndex++) {
			line.push_back({ BusStopName(static_cast<int>(lineIndex), static_cast<int>(stopIndex)), 0 });
		}
		lineVehicleCounts.push_back(max(1, static_cast<int>(line.size()) / kStationsPerVehicle));
		lines.push_back(std::move(line));
	}
}
