#include "suites.h"

#include <cmath>
#include <filesystem>
#include <fstream>

#include "test_framework.h"
#include "world_build.h"

#include "common/json.h"
#include "map/general_geometry.h"
#include "traffic/route.h"
#include "traffic/station.h"
#include "traffic/traffic.h"

using namespace std;

// transit套件：MapEditor"线路"工具设计的手动火车线路(Tools/MapEditor/samples/transit.roadnet，由tests/make_transit.js生成)。
//   平地上40x40方框路 + 一块居住地 + 2个火车站预设(s0在(100,100)一带、s1在(100,128)一带) + 实体铁轨 n0(40,120)->n1(80,120)平地、n1->n2(80,180,z=2)曲线桥(route_train)
//   + 一个线路环 [s0停, n0, n2, s1停]；config里route_train带--skip，route_mods里没有route_air。
//   检查：线路按环的停靠顺序规划成功(1条线、2段)、每段首尾相接且朝向连续、右手铁轨偏离中轴0.25(两轨0.5)、桥面升到节点高度。
// transit_twice：同一个车站在环里出现两次(往返各停一次)。车站轨道沿东西向，东西两侧各一条铁轨，
//   环 = [s0停, 东1, 东2, 东1, s0停, 西1, 西2, 西1]：规划成1条线、2段，两次停靠落在车站的两条不同轨道(t0向东、t1向西)，
//   两段都从s0出发回到s0，铁轨在东2/西2原路掉头。
// transit_hub：没有铁轨，两个孤立节点夹在s0两侧：环 = [s0停, 东点, s0停, 西点]，节点前后都是缺口且折回→要在节点处掉头，断言同transit_twice。
// transit_bus：MapEditor"路网路线"设计的手动公交线(config里route_bus带--skip)。方框路上顺时针4个站(每条边中点、行车方向右手边)成一个环：
//   station_bus按手动站建出4个BusStop_L0_S*(而不是自动的"每条4车道大路两侧各一站")、route_bus连成1条线4段、
//   每个站都挂在路上且不压在同侧的车辆开口上。
// 重新生成fixture后期望值要同步更新下面的常量。

namespace {
	constexpr float kJoinTolerance = 1e-3f;
	constexpr float kMaxKinkDegrees = 25.f;


	float YawDifference(float a, float b) {
		float d = fabsf(a - b);
		while (d > 360.f) d -= 360.f;
		return d > 180.f ? 360.f - d : d;
	}

	// 每段(run)里相邻片段首尾相接(位置)、接缝朝向不突变。
	void CheckJoints(const Route::RouteLine& line, bool differentStations) {
		TestRun& run = TestRun::Get();
		float maxKink = 0.f, maxGap = 0.f;
		size_t joints = 0;
		for (size_t r = 0; r < line.runs.size(); r++) {
			const Route::RouteRun& runLeg = line.runs[r];
			run.Check(runLeg.fromStation != nullptr && runLeg.toStation != nullptr && (!differentStations || runLeg.fromStation != runLeg.toStation),
				differentStations ? "a run joins two different stations" : "a run joins two stops", to_string(r));
			run.Check(runLeg.length > 1.f && runLeg.travelTime > 0.f, "a run has length and travel time", to_string(runLeg.length));
			for (size_t p = 0; p + 1 < runLeg.pieces.size(); p++) {
				const auto& a = runLeg.pieces[p].path;
				const auto& b = runLeg.pieces[p + 1].path;
				if (!a || !b) continue;
				float ax, ay, az, ayaw, apitch, bx, by, bz, byaw, bpitch;
				a->Evaluate(a->length, ax, ay, az, ayaw, apitch);
				b->Evaluate(0.f, bx, by, bz, byaw, bpitch);
				joints++;
				maxGap = max(maxGap, sqrtf((ax - bx) * (ax - bx) + (ay - by) * (ay - by) + (az - bz) * (az - bz)));
				maxKink = max(maxKink, YawDifference(ayaw, byaw));
			}
		}
		run.Check(joints >= 6, "the two runs are made of several pieces (stations, gap links, rails, turn)", to_string(joints));
		run.Metric("transit max joint gap", maxGap, "map units");
		run.Metric("transit max heading kink", maxKink, "deg");
		run.Check(maxGap < kJoinTolerance, "consecutive pieces of every run join end to start", to_string(maxGap));
		run.Check(maxKink < kMaxKinkDegrees, "no heading kink at the joints between pieces", to_string(maxKink) + " deg");

	}

}

void TransitSuiteBody(TestWorld& world, const TestOptions& options) {
	(void)options;
	TestRun& run = TestRun::Get();
	const Route* train = nullptr;
	for (const auto& [name, route] : world.traffic->GetRoutes()) {
		if (route && route->IsValid() && route->GetStationType() == "train") train = route;
	}
	run.Check(train != nullptr, "a valid train route exists");
	if (!train) return;

	const auto& lines = train->GetLines();
	run.Check(lines.size() == 1, "the editor loop became exactly one train line", to_string(lines.size()));
	if (lines.empty()) return;
	const Route::RouteLine& line = lines[0];
	run.Check(line.runs.size() == 2, "two stopping stations -> two runs (stop to stop, out and back)", to_string(line.runs.size()));
	run.Check(line.period > 0.f, "line period is positive", to_string(line.period));

	CheckJoints(line, true);

	// 铁轨：n0(40,120,z=0)的右手铁轨从(40,120.25)出发(+X方向的右手边是+Y)，左手铁轨到(40,119.75)结束，两轨相距0.5。
	bool rightStart = false, leftEnd = false;
	float bridgeTopZ = -1e9f;
	for (const auto& path : train->GetTrackPaths()) {
		if (!path || path->segments.empty()) continue;
		const TrackSegment& first = path->segments.front();
		const TrackSegment& last = path->segments.back();
		if (fabsf(first.x1 - 40.f) < 0.01f && fabsf(first.y1 - 120.25f) < 0.01f && fabsf(first.z1) < 0.01f) rightStart = true;
		if (fabsf(last.x2 - 40.f) < 0.01f && fabsf(last.y2 - 119.75f) < 0.01f && fabsf(last.z2) < 0.01f) leftEnd = true;
		for (const TrackSegment& seg : path->segments) bridgeTopZ = max(bridgeTopZ, max(seg.z1, seg.z2));
	}
	run.Check(rightStart, "right-hand rail leaves transit node 0 offset +0.25 to the right of the heading");
	run.Check(leftEnd, "left-hand rail arrives at transit node 0 offset 0.25 on the other side (rails 0.5 apart)");
	run.Check(bridgeTopZ > 1.99f, "the bridge reaches the height of its end node (z=2)", to_string(bridgeTopZ));
}

void TransitTwiceSuiteBody(TestWorld& world, const TestOptions& options) {
	(void)options;
	TestRun& run = TestRun::Get();
	const Route* train = nullptr;
	for (const auto& [name, route] : world.traffic->GetRoutes()) {
		if (route && route->IsValid() && route->GetStationType() == "train") train = route;
	}
	run.Check(train != nullptr, "a valid train route exists");
	if (!train) return;
	const auto& lines = train->GetLines();
	run.Check(lines.size() == 1, "the loop with the station twice became one train line", to_string(lines.size()));
	if (lines.empty()) return;
	const Route::RouteLine& line = lines[0];
	run.Check(line.runs.size() == 2, "the station stops twice -> two runs (out and back)", to_string(line.runs.size()));
	if (line.runs.size() != 2) return;
	const Route::RouteRun& a = line.runs[0];
	const Route::RouteRun& b = line.runs[1];
	run.Check(a.fromStation != nullptr && a.fromStation == a.toStation && b.fromStation == a.fromStation && b.toStation == a.fromStation,
		"both runs leave from and return to the same station");
	run.Check(a.fromStop != a.toStop, "the two stops of that station use different tracks", to_string(a.fromStop) + " -> " + to_string(a.toStop));
	run.Check(b.fromStop == a.toStop && b.toStop == a.fromStop, "the second run goes back from the other track to the first");
	run.Check(a.length > 20.f && b.length > 20.f, "each run goes out to the line end and back", to_string(a.length) + " / " + to_string(b.length));
	CheckJoints(line, false);
}

void TransitBusSuiteBody(TestWorld& world, const TestOptions& options) {
	(void)options;
	TestRun& run = TestRun::Get();
	const Route* bus = nullptr;
	for (const auto& [name, route] : world.traffic->GetRoutes()) {
		if (route && route->IsValid() && route->GetStationType() == "bus") bus = route;
	}
	run.Check(bus != nullptr, "a valid bus route exists");
	if (!bus) return;

	// 手动站：只有编辑器画的4个，没有自动的"4车道大路两侧各一站"。
	int manualStations = 0, otherBusStations = 0, overlaps = 0;
	for (const auto& [name, station] : world.traffic->GetStations()) {
		if (!station || !station->IsValid() || station->GetStationType() != "bus") continue;
		if (name.rfind("BusStop_L0_S", 0) != 0) { otherBusStations++; continue; }
		manualStations++;
		const Road* road = station->GetAttachedRoad();
		run.Check(road != nullptr && station->GetStops().size() == 1, "a manual bus stop is attached to a road with one stop", name);
		if (!road || station->GetStops().empty()) continue;

		// 站位在路上的沿路距离和所在侧：采样中心线找最近点(side0=沿a->b的右手边=(-ty,tx))
		const StationPort& stop = station->GetStops()[0];
		float length = road->CalcDistance();
		float bestDistance = 1e30f, bestT = 0.f, side = 0.f;
		for (int i = 0; i <= 200; i++) {
			float t = i / 200.f;
			Node p = road->GetPoint(t);
			float distance = (p.GetX() - stop.x) * (p.GetX() - stop.x) + (p.GetY() - stop.y) * (p.GetY() - stop.y);
			if (distance < bestDistance) {
				bestDistance = distance; bestT = t;
				float dx, dy, dz;
				road->GetTangent(t, dx, dy, dz);
				side = (stop.x - p.GetX()) * (-dy) + (stop.y - p.GetY()) * dx;
			}
		}
		bool forwardSide = side >= 0.f;
		for (const RoadOpening& opening : road->GetOpenings()) {
			if (!opening.isVehicle || opening.forwardSide != forwardSide) continue;
			if (fabsf(opening.t * length - bestT * length) < opening.width * 0.5f) overlaps++;
		}
	}
	run.Check(manualStations == 4, "exactly the 4 editor-drawn bus stops exist", to_string(manualStations));
	run.Check(otherBusStations == 0, "the manual stops replace the automatic roadside stations", to_string(otherBusStations));
	run.Check(overlaps == 0, "no manual bus stop sits on a same-side vehicle opening", to_string(overlaps));

	const auto& lines = bus->GetLines();
	run.Check(lines.size() == 1, "the editor bus line became exactly one line", to_string(lines.size()));
	if (lines.empty()) return;
	const Route::RouteLine& line = lines[0];
	run.Check(line.runs.size() == 4, "four stops -> four runs around the loop", to_string(line.runs.size()));
	run.Check(line.period > 0.f && line.vehicleCount >= 1, "the line has a period and at least one bus", to_string(line.period) + " / " + to_string(line.vehicleCount));
	for (size_t r = 0; r < line.runs.size(); r++) {
		const Route::RouteRun& leg = line.runs[r];
		run.Check(leg.fromStation && leg.toStation && leg.fromStation != leg.toStation, "a run joins two different bus stops", to_string(r));
		run.Check(leg.length > 1.f, "a run has length", to_string(leg.length));
	}
}

namespace {
	// 派生一份config：路网/地形换成sample，route_mods只留route_train --skip。
	bool WriteTransitConfig(const TestOptions& options, const string& stem, filesystem::path& derived, const string& routeMod = "route_train --skip") {
		TestRun& run = TestRun::Get();
		filesystem::path samples = filesystem::path(options.sourceRoot).parent_path() / "Tools" / "MapEditor" / "samples";
		const filesystem::path roadnet = samples / (stem + ".roadnet"), terrain = samples / (stem + ".terrain");
		if (!filesystem::exists(roadnet) || !filesystem::exists(terrain)) {
			run.Check(false, "transit map files exist", roadnet.string() + " / " + terrain.string());
			return false;
		}
		JsonValue root;
		{
			ifstream in(options.configPath);
			JsonReader reader;
			if (!in || !reader.Parse(in, root)) {
				run.Check(false, "read base config", options.configPath);
				return false;
			}
		}
		auto slashes = [](string text) { for (char& c : text) if (c == '\\') c = '/'; return text; };
		JsonValue roadnets(DATA_ARRAY), terrains(DATA_ARRAY), routes(DATA_ARRAY);
		roadnets.append(JsonValue("roadnet_file --path=" + slashes(roadnet.string())));
		terrains.append(JsonValue("terrain_file --path=" + slashes(terrain.string())));
		routes.append(JsonValue(routeMod));
		root["roadnet_mods"] = roadnets;
		root["terrain_mods"] = terrains;
		root["route_mods"] = routes;
		error_code ec;
		const filesystem::path dir = filesystem::path(options.outDir) / "child";
		filesystem::create_directories(dir, ec);
		derived = dir / ("config_" + stem + ".json");
		ofstream out(derived, ios::binary);
		StyledWriter writer;
		out << writer.Write(root);
		return true;
	}
}

void RunTransitSuite(const TestOptions& options) {
	TestRun& run = TestRun::Get();
	run.Begin("transit", 0, options.selfCommand + " --suite transit");
	for (const char* stem : { "transit", "transit_twice", "transit_hub", "transit_bus" }) {
		filesystem::path derived;
		if (!WriteTransitConfig(options, stem, derived, string(stem) == "transit_bus" ? "route_bus --skip" : "route_train --skip")) continue;
		TestOptions copy = options;
		copy.configPath = derived.string();
		RunSuiteInChildren(copy, stem, 256);
	}
}
