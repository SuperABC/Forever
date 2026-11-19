#include "suites.h"

#include "test_framework.h"
#include "world_build.h"

#include "map/map.h"
#include "map/building.h"
#include "map/room.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

using namespace std;

// route套件(子进程里)：随机行人在房间之间的寻路、车辆在停车位之间的寻路，各自的耗时分布(分位数+直方图)和失败率。
// 样本用世界种子派生的固定伪随机序列选取，所以同种子可复现。

namespace {
	struct SlotRef { Room* room; ParkingSlot* slot; };

	// 简单64位LCG，样本选取不依赖Core的随机流(避免计入unscoped抽取)。
	struct Pick {
		unsigned long long state;
		explicit Pick(unsigned long long seed) : state(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
		size_t Next(size_t n) {
			state = state * 6364136223846793005ULL + 1442695040888963407ULL;
			return static_cast<size_t>((state >> 33) % n);
		}
	};

	double Percentile(const vector<double>& sorted, double p) {
		if (sorted.empty()) return 0;
		return sorted[min(sorted.size() - 1, static_cast<size_t>(sorted.size() * p))];
	}

	// 记录mean/p50/p90/p95/p99/max，并把样本落到对数分桶的直方图里(每个桶一个Metric)。
	void ReportDistribution(const string& name, vector<double> millis) {
		TestRun& run = TestRun::Get();
		if (millis.empty()) return;
		sort(millis.begin(), millis.end());
		double sum = 0;
		for (double v : millis) sum += v;
		run.Metric(name + " mean", sum / millis.size(), "ms");
		run.Metric(name + " p50", Percentile(millis, 0.50), "ms");
		run.Metric(name + " p90", Percentile(millis, 0.90), "ms");
		run.Metric(name + " p95", Percentile(millis, 0.95), "ms");
		run.Metric(name + " p99", Percentile(millis, 0.99), "ms");
		run.Metric(name + " max", millis.back(), "ms");
		const double edges[] = { 0.01, 0.1, 1, 10, 100, 1000 };
		const char* labels[] = { "<0.01ms", "0.01-0.1ms", "0.1-1ms", "1-10ms", "10-100ms", "100ms-1s", ">=1s" };
		int counts[7] = {};
		for (double v : millis) {
			int bucket = 0;
			while (bucket < 6 && v >= edges[bucket]) bucket++;
			counts[bucket]++;
		}
		for (int i = 0; i < 7; i++) run.Metric(name + " hist " + labels[i], counts[i], "");
	}

	void MeasurePedestrians(TestWorld& world, const TestOptions& options, unsigned long long seed) {
		TestRun& run = TestRun::Get();
		vector<Room*> rooms;
		for (const auto& [name, building] : world.map->GetBuildings()) {
			for (Room* room : building->GetRooms()) if (room->GetNavigationNode()) rooms.push_back(room);
		}
		run.Metric("pedestrian rooms", static_cast<double>(rooms.size()), "");
		run.Check(rooms.size() > 1, "world has rooms with pedestrian nodes");
		if (rooms.size() < 2) return;
		const int samples = options.quick ? 300 : 2000;
		Pick pick(seed ^ 0x9e3779b97f4a7c15ULL);
		vector<double> all, okOnly;
		int failed = 0, same = 0;
		size_t totalPoints = 0;
		for (int i = 0; i < samples; i++) {
			Room* from = rooms[pick.Next(rooms.size())];
			Room* to = rooms[pick.Next(rooms.size())];
			if (from == to) same++;
			const auto begin = chrono::steady_clock::now();
			const Map::VehicleRoutePlan plan = world.map->PlanPedestrianRoute(from, 0.f, 0.f, to);
			const double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - begin).count();
			all.push_back(ms);
			if (plan.status == Map::VehicleRoutePlan::OK) { okOnly.push_back(ms); totalPoints += plan.points.size(); }
			else failed++;
		}
		ReportDistribution("pedestrian route(all)", all);
		ReportDistribution("pedestrian route(ok)", okOnly);
		run.Metric("pedestrian route failures", failed, "");
		run.Metric("pedestrian route same-room samples", same, "");
		if (!okOnly.empty()) run.Metric("pedestrian route avg points", static_cast<double>(totalPoints) / okOnly.size(), "");
		run.Check(failed * 20 <= samples, "at most 5% of random room-to-room pedestrian routes fail",
			to_string(failed) + "/" + to_string(samples));
	}

	void MeasureVehicles(TestWorld& world, const TestOptions& options, unsigned long long seed) {
		TestRun& run = TestRun::Get();
		vector<SlotRef> slots;
		for (const auto& [name, building] : world.map->GetBuildings()) {
			for (Room* room : building->GetRooms()) {
				for (ParkingSlot* slot : room->GetParkingSlots()) if (slot->navigationNode) slots.push_back({ room, slot });
			}
		}
		run.Metric("parking slots", static_cast<double>(slots.size()), "");
		run.Check(slots.size() > 1, "world has parking slots to route between");
		if (slots.size() < 2) return;
		const int samples = options.quick ? 300 : 2000;
		Pick pick(seed ^ 0xc2b2ae3d27d4eb4fULL);
		vector<double> all, okOnly;
		int failed = 0;
		size_t totalPoints = 0;
		for (int i = 0; i < samples; i++) {
			const SlotRef& from = slots[pick.Next(slots.size())];
			const SlotRef& to = slots[pick.Next(slots.size())];
			const auto begin = chrono::steady_clock::now();
			const Map::VehicleRoutePlan plan = world.map->PlanVehicleRouteFromSlot(from.room, from.slot, to.slot->navigationNode->GetId(), 4.5f);
			const double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - begin).count();
			all.push_back(ms);
			if (plan.status == Map::VehicleRoutePlan::OK) { okOnly.push_back(ms); totalPoints += plan.points.size(); }
			else failed++;
		}
		ReportDistribution("vehicle route(all)", all);
		ReportDistribution("vehicle route(ok)", okOnly);
		run.Metric("vehicle route failures", failed, "");
		if (!okOnly.empty()) run.Metric("vehicle route avg points", static_cast<double>(totalPoints) / okOnly.size(), "");
		run.Check(failed * 20 <= samples, "at most 5% of random slot-to-slot vehicle routes fail",
			to_string(failed) + "/" + to_string(samples));
	}
}

void RouteSuiteBody(TestWorld& world, const TestOptions& options) {
	const unsigned long long seed = options.childSeed ? options.childSeed : 1;
	MeasurePedestrians(world, options, seed);
	MeasureVehicles(world, options, seed);
}

void RunRouteSuite(const TestOptions& options) {
	TestRun::Get().Begin("route", 0, options.selfCommand + " --suite route");
	RunSuiteInChildren(options, "route", options.EffectiveMapSize());
}
