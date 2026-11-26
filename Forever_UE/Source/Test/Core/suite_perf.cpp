#include "suites.h"

#include "test_framework.h"
#include "world_build.h"

#include "common/json.h"
#include "common/world_step.h"
#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "populace/populace.h"
#include "player/player.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// perf套件(子进程里)：车行寻路耗时分布、每日tick耗时分布，与 Source/Test/budgets.json 对比。
// 预算只告警(Note)，不判失败，首版基线稳定后再收紧。

namespace {
	struct Stats { double p50 = 0, p95 = 0, max = 0; };

	Stats Summarize(vector<double>& samples) {
		Stats stats;
		if (samples.empty()) return stats;
		sort(samples.begin(), samples.end());
		stats.p50 = samples[samples.size() / 2];
		stats.p95 = samples[min(samples.size() - 1, samples.size() * 95 / 100)];
		stats.max = samples.back();
		return stats;
	}

	double LoadBudget(const TestOptions& options, const string& key) {
		ifstream in(options.sourceRoot + "/Test/budgets.json");
		if (!in) return 0;
		stringstream buffer;
		buffer << in.rdbuf();
		const string text = buffer.str();
		const size_t at = text.find("\"" + key + "\"");
		if (at == string::npos) return 0;
		const size_t colon = text.find(':', at);
		return colon == string::npos ? 0 : atof(text.c_str() + colon + 1);
	}

	void Report(const TestOptions& options, const string& name, const Stats& stats, const char* unit) {
		TestRun& run = TestRun::Get();
		run.Metric(name + " p50", stats.p50, unit);
		run.Metric(name + " p95", stats.p95, unit);
		run.Metric(name + " max", stats.max, unit);
		const double budget = LoadBudget(options, name + ".p95");
		if (budget > 0 && stats.p95 > budget) {
			run.Note("PERF WARNING: " + name + " p95 " + to_string(stats.p95) + unit + " exceeds budget " + to_string(budget) + unit);
		}
	}

	struct SlotRef { Room* room; ParkingSlot* slot; };

	void MeasureRouting(TestWorld& world, const TestOptions& options) {
		TestRun& run = TestRun::Get();
		vector<SlotRef> slots;
		for (const auto& [name, building] : world.map->GetBuildings()) {
			for (Room* room : building->GetRooms()) {
				// offRoad车位(船位/机位这类)不在车行图里，不参与公路寻路抽样；它们的核对在route套件。
				for (ParkingSlot* slot : room->GetParkingSlots()) if (slot->navigationNode && !slot->offRoad) slots.push_back({ room, slot });
			}
		}
		run.Check(slots.size() > 1, "world has parking slots to route between");
		if (slots.size() < 2) return;
		const int samples = options.quick ? 200 : 1000;
		vector<double> millis;
		int failed = 0;
		for (int i = 0; i < samples; i++) {
			const SlotRef& from = slots[(static_cast<size_t>(i) * 7919) % slots.size()];
			const SlotRef& to = slots[(static_cast<size_t>(i) * 104729 + 13) % slots.size()];
			const auto begin = chrono::steady_clock::now();
			const Map::VehicleRoutePlan plan = world.map->PlanVehicleRouteFromSlot(from.room, from.slot, to.slot->navigationNode->GetId(), 4.5f);
			millis.push_back(chrono::duration<double, milli>(chrono::steady_clock::now() - begin).count());
			if (plan.status != Map::VehicleRoutePlan::OK) failed++;
		}
		Stats stats = Summarize(millis);
		Report(options, "route", stats, "ms");
		run.Metric("route failures", failed, "");
		run.Check(failed * 20 <= samples, "at most 5% of random slot-to-slot routes fail", to_string(failed) + "/" + to_string(samples));
	}

	void MeasureDailyTick(TestWorld& world, const TestOptions& options) {
		WorldDomains domains;
		domains.map = world.map;
		domains.populace = world.populace;
		domains.society = world.society;
		domains.industry = world.industry;
		domains.traffic = world.traffic;
		domains.story = world.story;
		domains.player = world.player;
		domains.polit = world.polit;
		vector<double> seconds;
		const int days = options.quick ? 10 : 30;
		for (int day = 0; day < days; day++) {
			Time next = *world.player->GetTime();
			next.SetTime(0, 0, 0, 0);
			next.AddDays(1);
			world.player->SetTime(next);
			const bool crossed = world.player->CrossDay();
			const auto begin = chrono::steady_clock::now();
			WorldStep::Step(domains, crossed, WorldStepHooks());
			seconds.push_back(chrono::duration<double>(chrono::steady_clock::now() - begin).count());
		}
		Report(options, "daily_tick", Summarize(seconds), "s");
		// 规模外推：按每千市民的单日耗时。
		const double citizens = static_cast<double>(world.populace->GetCitizens().size());
		if (citizens > 0) TestRun::Get().Metric("daily tick per 1000 citizens", Summarize(seconds).p50 / citizens * 1000.0, "s");
	}
}

void PerfSuiteBody(TestWorld& world, const TestOptions& options) {
	for (const WorldStage& stage : world.stages) {
		TestRun::Get().Metric("generate." + stage.name, stage.seconds, "s");
		const double budget = LoadBudget(options, "generate." + stage.name);
		if (budget > 0 && stage.seconds > budget) {
			TestRun::Get().Note("PERF WARNING: generate." + stage.name + " " + to_string(stage.seconds) + "s exceeds budget " + to_string(budget) + "s");
		}
	}
	RecordMemory("perform: before routing");
	MeasureRouting(world, options);
	RecordMemory("perform: after routing");
	MeasureDailyTick(world, options);
	RecordMemory("perform: after daily ticks");
}

void RunPerfSuite(const TestOptions& options) {
	TestRun::Get().Begin("perform", 0, options.selfCommand + " --suite perform");
	RunSuiteInChildren(options, "perform", options.EffectiveMapSize());
}
