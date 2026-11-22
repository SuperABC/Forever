#include "suites.h"

#include "test_framework.h"
#include "world_build.h"

#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "player/player.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <set>
#include <string>
#include <vector>

using namespace std;

// route套件(子进程里)：随机行人在房间之间的寻路、车辆在停车位之间的寻路，各自的耗时分布(分位数+直方图)和失败率；
// 行人路线缓存：冷/热/反方向三轮耗时、命中与一致性、条目数与内存、25游戏小时过期。
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

	double PolylineLength(const Map::VehicleRoutePlan& plan) {
		double total = 0;
		for (size_t i = 1; i < plan.points.size(); i++) {
			const double dx = plan.points[i].x - plan.points[i - 1].x, dy = plan.points[i].y - plan.points[i - 1].y, dz = plan.points[i].z - plan.points[i - 1].z;
			total += sqrt(dx * dx + dy * dy + dz * dz);
		}
		return total;
	}

	bool SamePlan(const Map::VehicleRoutePlan& a, const Map::VehicleRoutePlan& b) {
		if (a.status != b.status || a.points.size() != b.points.size() || a.edges.size() != b.edges.size()) return false;
		for (size_t i = 0; i < a.points.size(); i++) {
			if (a.points[i].x != b.points[i].x || a.points[i].y != b.points[i].y || a.points[i].z != b.points[i].z) return false;
		}
		for (size_t i = 0; i < a.edges.size(); i++) {
			if (a.edges[i].edge != b.edges[i].edge || a.edges[i].firstPoint != b.edges[i].firstPoint || a.edges[i].lastPoint != b.edges[i].lastPoint) return false;
		}
		return true;
	}

	// 结构自洽：edges的点下标范围按序首尾相接，且在points范围内。
	bool EdgesContiguous(const Map::VehicleRoutePlan& plan) {
		for (size_t i = 0; i < plan.edges.size(); i++) {
			const auto& edge = plan.edges[i];
			if (edge.firstPoint < 0 || edge.lastPoint >= static_cast<int>(plan.points.size()) || edge.firstPoint > edge.lastPoint) return false;
			if (i > 0 && plan.edges[i - 1].lastPoint != edge.firstPoint) return false;
		}
		return true;
	}

	void MeasurePedestrians(TestWorld& world, const TestOptions& options, unsigned long long seed) {
		TestRun& run = TestRun::Get();
		vector<Room*> rooms;
		for (const auto& [name, building] : world.map->GetBuildings()) {
			for (Room* room : building->GetRooms()) if (room->GetNavigationNode()) rooms.push_back(room);
		}
		run.Metric("pedestrian rooms", static_cast<double>(rooms.size()), "");
		{
			// 行人边全是双向的：a->b存在则b->a也必须存在，否则正反向寻路结果不同。
			const NavGraph& graph = world.map->GetPedestrianNavGraph();
			int asymmetric = 0;
			for (const auto& [from, edges] : graph) {
				for (const auto& [to, connection] : edges) {
					bool reverseExists = false;
					auto back = graph.find(to);
					if (back != graph.end()) for (const auto& [target, c2] : back->second) if (target == from) { reverseExists = true; break; }
					if (reverseExists) continue;
					if (asymmetric++ < 8) {
						const Node s = connection->GetStart(), e = connection->GetEnd();
						run.Note("one-way pedestrian edge " + to_string(from) + " -> " + to_string(to) + " conn start=" + to_string(s.GetId()) + "(" + to_string(s.GetX()) + "," + to_string(s.GetY()) + ")"
							+ " end=" + to_string(e.GetId()) + "(" + to_string(e.GetX()) + "," + to_string(e.GetY()) + ") len=" + to_string(connection->CalcDistance()));
					}
				}
			}
			run.Metric("pedestrian one-way edges", asymmetric, "");
			run.Check(asymmetric == 0, "pedestrian graph edges are all two-way", to_string(asymmetric) + " one-way");
			// 每个有导航节点的房间都必须接进行人图：不在图里的房间，作为终点永远NO_PATH，作为起点则退化成"取离原点最近的节点"算出凭空的路线。
			int isolated = 0;
			for (Room* room : rooms) {
				if (graph.count(room->GetNavigationNode()->GetId()) > 0) continue;
				if (isolated++ < 12) run.Note("room not connected to the pedestrian graph: " + room->GetAddress() + " (" + room->GetName() + ") node=" + to_string(room->GetNavigationNode()->GetId()));
			}
			run.Metric("pedestrian isolated rooms", isolated, "");
			run.Check(isolated == 0, "every room with a navigation node is connected to the pedestrian graph", to_string(isolated) + " isolated");
		}
		run.Check(rooms.size() > 1, "world has rooms with pedestrian nodes");
		if (rooms.size() < 2) return;
		const int samples = options.quick ? 300 : 2000;
		Pick pick(seed ^ 0x9e3779b97f4a7c15ULL);
		vector<pair<Room*, Room*>> pairs;
		for (int i = 0; i < samples; i++) {
			Room* from = rooms[pick.Next(rooms.size())];
			Room* to = rooms[pick.Next(rooms.size())];
			if (from != to) pairs.push_back({ from, to });
		}

		// 以下按"冷缓存 -> 同方向再来一遍 -> 反方向"三轮跑同一批起终点。
		world.map->ClearPedestrianRouteCache();
		RecordMemory("route: pedestrian cache empty");
		const Map::RouteCacheStats empty = world.map->GetPedestrianRouteCacheStats();

		vector<Map::VehicleRoutePlan> cold;
		vector<double> coldMs, coldOk;
		int failed = 0;
		size_t totalPoints = 0;
		for (const auto& [from, to] : pairs) {
			const auto begin = chrono::steady_clock::now();
			Map::VehicleRoutePlan plan = world.map->PlanPedestrianRoute(from, 0.f, 0.f, to);
			const double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - begin).count();
			coldMs.push_back(ms);
			if (plan.status == Map::VehicleRoutePlan::OK) { coldOk.push_back(ms); totalPoints += plan.points.size(); }
			else failed++;
			cold.push_back(std::move(plan));
		}
		ReportDistribution("pedestrian route cold(all)", coldMs);
		ReportDistribution("pedestrian route cold(ok)", coldOk);
		run.Metric("pedestrian route failures", failed, "");
		if (!coldOk.empty()) run.Metric("pedestrian route avg points", static_cast<double>(totalPoints) / coldOk.size(), "");
		run.Check(failed * 20 <= static_cast<int>(pairs.size()), "at most 5% of random room-to-room pedestrian routes fail",
			to_string(failed) + "/" + to_string(pairs.size()));
		const Map::RouteCacheStats afterCold = world.map->GetPedestrianRouteCacheStats();
		RecordMemory("route: pedestrian cache after cold pass");

		// 同方向再来一遍：全部命中、结果逐点相同、不再有misses。
		vector<double> warmMs;
		int different = 0;
		for (size_t i = 0; i < pairs.size(); i++) {
			const auto begin = chrono::steady_clock::now();
			const Map::VehicleRoutePlan plan = world.map->PlanPedestrianRoute(pairs[i].first, 0.f, 0.f, pairs[i].second);
			warmMs.push_back(chrono::duration<double, milli>(chrono::steady_clock::now() - begin).count());
			if (!SamePlan(plan, cold[i])) different++;
		}
		ReportDistribution("pedestrian route warm(all)", warmMs);
		const Map::RouteCacheStats afterWarm = world.map->GetPedestrianRouteCacheStats();
		run.Check(different == 0, "cached pedestrian route is identical to the freshly computed one", to_string(different) + " differ");
		run.Check(afterWarm.misses == afterCold.misses, "second pass over the same pairs has no cache misses",
			to_string(afterWarm.misses - afterCold.misses) + " misses");
		run.Check(afterWarm.entries == afterCold.entries, "second pass adds no cache entries");

		// 反方向：A->B和B->A共用一条，不应再有misses和新条目；结果是倒过来的路线。
		vector<double> reverseMs;
		int badReverse = 0;
		for (size_t i = 0; i < pairs.size(); i++) {
			const auto begin = chrono::steady_clock::now();
			const Map::VehicleRoutePlan plan = world.map->PlanPedestrianRoute(pairs[i].second, 0.f, 0.f, pairs[i].first);
			reverseMs.push_back(chrono::duration<double, milli>(chrono::steady_clock::now() - begin).count());
			const Map::VehicleRoutePlan& forward = cold[i];
			bool ok = plan.status == forward.status;
			if (ok && forward.status == Map::VehicleRoutePlan::OK) {
				ok = plan.points.size() == forward.points.size() && plan.edges.size() == forward.edges.size() && EdgesContiguous(plan);
				if (ok && !plan.points.empty()) {
					const auto& first = plan.points.front();
					const auto& last = forward.points.back();
					ok = first.x == last.x && first.y == last.y && first.z == last.z;
					const double lengthA = PolylineLength(plan), lengthB = PolylineLength(forward);
					ok = ok && fabs(lengthA - lengthB) <= 1e-3 * (1.0 + lengthB);
				}
			}
			if (!ok) {
				badReverse++;
				run.Note("reverse mismatch: " + pairs[i].first->GetAddress() + " (" + pairs[i].first->GetName() + ") -> " + pairs[i].second->GetAddress()
					+ " (" + pairs[i].second->GetName() + ") forward status=" + to_string(static_cast<int>(forward.status)) + " points=" + to_string(forward.points.size())
					+ " reverse status=" + to_string(static_cast<int>(plan.status)) + " points=" + to_string(plan.points.size())
					+ " fromNode=" + to_string(pairs[i].first->GetNavigationNode()->GetId()) + " toNode=" + to_string(pairs[i].second->GetNavigationNode()->GetId()));
			}
		}
		ReportDistribution("pedestrian route reverse(all)", reverseMs);
		const Map::RouteCacheStats afterReverse = world.map->GetPedestrianRouteCacheStats();
		run.Check(badReverse == 0, "reverse-direction route is the forward route reversed (same status/length, endpoints swapped, contiguous edges)",
			to_string(badReverse) + " bad");
		run.Check(afterReverse.misses == afterWarm.misses, "reverse direction hits the same cache entry (no new misses)",
			to_string(afterReverse.misses - afterWarm.misses) + " misses");
		run.Check(afterReverse.entries == afterWarm.entries, "reverse direction adds no cache entries");

		run.Metric("pedestrian cache entries", static_cast<double>(afterReverse.entries), "");
		run.Metric("pedestrian cache bytes", static_cast<double>(afterReverse.bytes) - static_cast<double>(empty.bytes), "B");
		if (afterReverse.entries > 0) run.Metric("pedestrian cache bytes per entry", static_cast<double>(afterReverse.bytes) / afterReverse.entries, "B");
		run.Metric("pedestrian cache hits", static_cast<double>(afterReverse.hits), "");
		run.Metric("pedestrian cache misses", static_cast<double>(afterReverse.misses), "");

		// 过期：25游戏小时没命中才清。T0写入/命中，T0+24h还在；刷新其中一部分，T0+26h时只有被刷新的留下。
		const Time t0 = *world.player->GetTime();
		world.map->Tick(t0, false, nullptr);
		for (const auto& [from, to] : pairs) world.map->PlanPedestrianRoute(from, 0.f, 0.f, to); // 命中并把lastHit刷到T0
		Time t24 = t0;
		t24.AddHours(24);
		world.map->Tick(t24, false, nullptr);
		run.Check(world.map->GetPedestrianRouteCacheStats().entries == afterReverse.entries, "entries survive 24h without a hit");
		const size_t refresh = min<size_t>(pairs.size(), 10);
		for (size_t i = 0; i < refresh; i++) world.map->PlanPedestrianRoute(pairs[i].first, 0.f, 0.f, pairs[i].second); // lastHit -> T0+24h
		Time t26 = t0;
		t26.AddHours(26);
		world.map->Tick(t26, false, nullptr);
		const Map::RouteCacheStats afterPurge = world.map->GetPedestrianRouteCacheStats();
		run.Check(afterPurge.entries <= refresh && afterPurge.entries > 0 && afterPurge.entries < afterReverse.entries,
			"entries not hit for more than 25h are purged, recently hit ones kept",
			to_string(afterPurge.entries) + " left of " + to_string(afterReverse.entries) + " (refreshed pairs " + to_string(refresh) + ")");
		run.Metric("pedestrian cache purged", static_cast<double>(afterPurge.purged), "");
		RecordMemory("route: pedestrian cache after purge");
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
		const Map::RouteCacheStats cacheBefore = world.map->GetPedestrianRouteCacheStats();
		RecordMemory("route: before vehicle routes");
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
		RecordMemory("route: after vehicle routes"); // 车辆路线不缓存，这里内存不该随样本数增长
		const Map::RouteCacheStats cacheAfter = world.map->GetPedestrianRouteCacheStats();
		run.Check(cacheAfter.entries == cacheBefore.entries && cacheAfter.hits == cacheBefore.hits && cacheAfter.misses == cacheBefore.misses,
			"vehicle routing does not touch the pedestrian route cache");
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
