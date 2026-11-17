#include "suites.h"

#include "test_framework.h"
#include "world_build.h"

#include "common/random.h"
#include "common/save.h"
#include "common/save_archive.h"
#include "common/world_step.h"
#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "map/map_save.h"
#include "populace/populace.h"
#include "populace/populace_save.h"
#include "society/society.h"
#include "society/society_save.h"
#include "player/player.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace std;

// 存档套件(在子进程里、世界生成并推进几天之后运行)：存档幂等、容器序列化/解析、截断与损坏不崩、
// 人口/社会段读回再存、随机流状态覆盖。mod名计数器会让带mod名字的段字节有差异，所以社会段只比账本部分。

namespace {
	SaveWorld ToSaveWorld(const TestWorld& world) {
		SaveWorld save;
		save.map = world.map;
		save.populace = world.populace;
		save.society = world.society;
		save.industry = world.industry;
		save.traffic = world.traffic;
		save.story = world.story;
		save.player = world.player;
		return save;
	}

	void AdvanceDays(TestWorld& world, int days) {
		WorldDomains domains;
		domains.map = world.map;
		domains.populace = world.populace;
		domains.society = world.society;
		domains.industry = world.industry;
		domains.traffic = world.traffic;
		domains.story = world.story;
		domains.player = world.player;
		for (int day = 0; day < days; day++) {
			Time next = *world.player->GetTime();
			next.SetTime(0, 0, 0, 0);
			next.AddDays(1);
			world.player->SetTime(next);
			const bool crossed = world.player->CrossDay();
			WorldStep::Step(domains, crossed, WorldStepHooks());
		}
	}

	// 两份存档逐段字节比较，返回第一处不同的段名(都一样返回空串)。
	string FirstDifferentSection(const SaveArchive& a, const SaveArchive& b) {
		if (a.GetSections().size() != b.GetSections().size()) return "section count";
		for (const SaveSection& section : a.GetSections()) {
			const SaveSection* other = b.Find(section.name);
			if (!other) return section.name + " (missing)";
			if (section.data != other->data) return section.name;
		}
		return "";
	}

	void CheckArchiveContainer(const SaveArchive& archive) {
		TestRun& run = TestRun::Get();
		const vector<uint8_t> bytes = archive.Serialize();
		run.Metric("archive bytes", static_cast<double>(bytes.size()), "B");
		SaveArchive parsed;
		string error;
		run.Check(parsed.Parse(bytes.data(), bytes.size(), error), "archive Parse accepts its own Serialize", error);
		run.Check(parsed.Serialize() == bytes, "archive Serialize->Parse->Serialize is byte identical");

		// 截断：任何长度的前缀要么被拒绝要么解析成功，但不能崩(崩=整个套件失败)。
		int accepted = 0;
		for (size_t k = 0; k < 40; k++) {
			const size_t length = bytes.size() * k / 40;
			SaveArchive truncated;
			string truncError;
			if (truncated.Parse(bytes.data(), length, truncError)) accepted++;
		}
		run.Check(true, "truncated archives never crash Parse", to_string(accepted) + "/40 prefixes accepted");
		run.Check(accepted < 40, "most truncated archives are rejected");

		// 小型fuzz：翻转头部和随机位置的字节。
		unsigned long long state = 0x9E3779B97F4A7C15ULL;
		for (int k = 0; k < 200; k++) {
			vector<uint8_t> broken = bytes;
			state = state * 6364136223846793005ULL + 1442695040888963407ULL;
			const size_t position = k < 60 ? (state >> 33) % min<size_t>(broken.size(), 64) : (state >> 33) % broken.size();
			broken[position] ^= static_cast<uint8_t>(1u << ((state >> 20) % 8));
			SaveArchive fuzzed;
			string fuzzError;
			fuzzed.Parse(broken.data(), broken.size(), fuzzError);
		}
		run.Check(true, "200 single-bit corruptions never crash Parse");
	}

	void CheckPopulaceRoundTrip(TestWorld& world) {
		TestRun& run = TestRun::Get();
		SaveArchive first;
		MapSaveCodec::Save(*world.map, first);
		PopulaceSaveCodec::Save(*world.populace, *world.map, nullptr, first);
		string error;
		Map* map2 = MapSaveCodec::Load(first, error);
		run.Check(map2 != nullptr, "map section loads back", error);
		if (!map2) return;
		vector<pair<JobExperience*, int>> pending;
		Populace* populace2 = PopulaceSaveCodec::Load(first, *map2, pending, error);
		run.Check(populace2 != nullptr, "populace section loads back", error);
		if (!populace2) return;
		SaveArchive second;
		PopulaceSaveCodec::Save(*populace2, *map2, nullptr, second);
		run.Check(first.Find(SAVE_SECTION_POPULACE)->data == second.Find(SAVE_SECTION_POPULACE)->data,
			"populace section round trip is byte identical");
		run.Check(populace2->GetCitizens().size() == world.populace->GetCitizens().size(), "citizen count survives the round trip");
	}

	void CheckSocietyRoundTrip(TestWorld& world) {
		TestRun& run = TestRun::Get();
		SaveArchive first;
		SocietySaveCodec::Save(*world.society, *world.map, *world.populace, first);
		string error;
		// 读档会把市民的Job重新指向新建的岗位，所以放在最后做。
		Society* restored = SocietySaveCodec::Load(first, *world.map, *world.populace, {}, error);
		run.Check(restored != nullptr, "society section loads back", error);
		if (!restored) return;
		SaveArchive second;
		SocietySaveCodec::Save(*restored, *world.map, *world.populace, second);
		run.Check(restored->GetLedger().Total() == 0, "restored ledger total is zero");
		run.Check(restored->GetLedger().CitizensTotal() == world.society->GetLedger().CitizensTotal(), "restored citizens funds match");
		run.Check(restored->GetLedger().OrganizationsTotal() == world.society->GetLedger().OrganizationsTotal(), "restored organization funds match");
		run.Check(restored->GetOrganizations().size() == world.society->GetOrganizations().size(), "organization count survives the round trip");

		const vector<uint8_t>& da = first.Find(SAVE_SECTION_SOCIETY)->data;
		const vector<uint8_t>& db = second.Find(SAVE_SECTION_SOCIETY)->data;
		size_t diff = 0;
		while (diff < da.size() && diff < db.size() && da[diff] == db[diff]) diff++;
		const size_t ledgerBytes = 8 + 8 + 4 + 4 + 12 * (world.society->GetLedger().GetCitizenBalances().size() + world.society->GetLedger().GetOrganizationBalances().size());
		const bool ledgerSame = da.size() >= ledgerBytes && db.size() >= ledgerBytes && equal(da.end() - ledgerBytes, da.end(), db.end() - ledgerBytes);
		run.Check(ledgerSame, "society ledger bytes round trip identical", "first difference at " + to_string(diff) + "/" + to_string(da.size()));
		run.Note(string("society section whole-section identical=") + (da == db ? "yes" : "no (mod name counters)"));
	}

	void CheckRandomRestore(TestWorld& world) {
		TestRun& run = TestRun::Get();
		const SaveArchive archive = SaveGame::BuildArchive(ToSaveWorld(world));
		const bool hasRandom = archive.Find(SAVE_SECTION_RANDOM) != nullptr;
		run.Check(hasRandom, "archive contains the world.random section");
		const vector<RandomRegistry::Entry> before = RandomRegistry::Snapshot();
		// 把所有域流抽一些，再用存档里的状态覆盖，应该回到抽之前的状态。
		{
			RandomScope scope(RandomRegistry::GetDomainStream("core.test.restore"));
			for (int k = 0; k < 100; k++) GetRandom(0, 1000);
		}
		SaveGame::RestoreRandom(archive);
		const vector<RandomRegistry::Entry> after = RandomRegistry::Snapshot();
		int mismatched = 0;
		for (const RandomRegistry::Entry& entry : before) {
			for (const RandomRegistry::Entry& other : after) {
				if (other.key != entry.key) continue;
				if (!equal(begin(entry.state), end(entry.state), begin(other.state))) mismatched++;
			}
		}
		run.Check(mismatched == 0, "RestoreRandom puts every saved stream back at its saved position", to_string(mismatched) + " streams");
	}
}

void SaveSuiteBody(TestWorld& world, const TestOptions& options) {
	(void)options;
	TestRun& run = TestRun::Get();
	AdvanceDays(world, 5); // 让存档里有行驶/计时器/账本流水，而不是刚生成的状态

	const SaveWorld saveWorld = ToSaveWorld(world);
	const SaveArchive first = SaveGame::BuildArchive(saveWorld);
	const SaveArchive second = SaveGame::BuildArchive(saveWorld);
	const string different = FirstDifferentSection(first, second);
	run.Check(different.empty(), "saving twice without changes gives identical sections", different);

	CheckArchiveContainer(first);
	CheckRandomRestore(world);
	CheckPopulaceRoundTrip(world);
	CheckSocietyRoundTrip(world);
}

void RunSaveSuite(const TestOptions& options) {
	TestRun::Get().Begin("save", 0, options.selfCommand + " --suite save");
	RunSuiteInChildren(options, "save", options.EffectiveMapSize());
}
