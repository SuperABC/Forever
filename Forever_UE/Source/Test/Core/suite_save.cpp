#include "suites.h"

#include "test_framework.h"
#include "world_build.h"

#include "common/random.h"
#include "common/save.h"
#include "polit/polit.h"
#include "polit/polit_save.h"
#include "common/save_archive.h"
#include "common/world_step.h"
#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "map/map_save.h"
#include "populace/populace.h"
#include "common/utility.h"
#include "common/implement.h"
#include "story/change.h"
#include "populace/citizen.h"
#include "populace/populace_save.h"
#include "society/society.h"
#include "society/society_save.h"
#include "player/player.h"
#include "player/player_save.h"
#include "player/asset.h"

#include <algorithm>
#include <cmath>
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
		save.polit = world.polit;
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
		domains.polit = world.polit;
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

	// 房间家具存读一致：读档不重新Layout，所以每个Room的家具(网格路径+位置+朝向+缩放)必须原样回来。
	void CheckFurnitureRoundTrip(TestWorld& world) {
		TestRun& run = TestRun::Get();
		SaveArchive archive;
		MapSaveCodec::Save(*world.map, archive);
		string error;
		Map* loaded = MapSaveCodec::Load(archive, error);
		run.Check(loaded != nullptr, "map loads back for the furniture check", error);
		if (!loaded) return;
		size_t total = 0, mismatchedRooms = 0, missingBuildings = 0;
		size_t slotTotal = 0, offRoadSlots = 0, mismatchedSlotRooms = 0;
		for (const auto& entry : world.map->GetBuildings()) {
			auto other = loaded->GetBuildings().find(entry.first);
			if (other == loaded->GetBuildings().end()) { missingBuildings++; continue; }
			const vector<Room*>& a = entry.second->GetRooms();
			const vector<Room*>& b = other->second->GetRooms();
			if (a.size() != b.size()) { mismatchedRooms++; continue; }
			for (size_t i = 0; i < a.size(); i++) {
				const vector<DecorationInstance>& fa = a[i]->GetFurniture();
				const vector<DecorationInstance>& fb = b[i]->GetFurniture();
				total += fa.size();
				bool same = fa.size() == fb.size();
				for (size_t k = 0; same && k < fa.size(); k++) {
					same = fa[k].meshPath == fb[k].meshPath && fa[k].x == fb[k].x && fa[k].y == fb[k].y && fa[k].z == fb[k].z &&
						fa[k].yaw == fb[k].yaw && fa[k].scaleX == fb[k].scaleX && fa[k].scaleY == fb[k].scaleY && fa[k].scaleZ == fb[k].scaleZ;
				}
				if (!same) mismatchedRooms++;
				// 车位也不重新Layout：数量、几何、可停种类、offRoad标记必须原样回来。
				const vector<ParkingSlot*>& sa = a[i]->GetParkingSlots();
				const vector<ParkingSlot*>& sb = b[i]->GetParkingSlots();
				slotTotal += sa.size();
				bool sameSlots = sa.size() == sb.size();
				for (size_t k = 0; sameSlots && k < sa.size(); k++) {
					if (sa[k]->offRoad) offRoadSlots++;
					sameSlots = sa[k]->centerX == sb[k]->centerX && sa[k]->centerY == sb[k]->centerY && sa[k]->halfWidth == sb[k]->halfWidth &&
						sa[k]->halfDepth == sb[k]->halfDepth && sa[k]->direction == sb[k]->direction && sa[k]->categories == sb[k]->categories &&
						sa[k]->offRoad == sb[k]->offRoad;
				}
				if (!sameSlots) mismatchedSlotRooms++;
			}
		}
		run.Metric("parking slots saved", static_cast<double>(slotTotal), "");
		run.Metric("off-road parking slots saved", static_cast<double>(offRoadSlots), "");
		run.Check(mismatchedSlotRooms == 0, "parking slots (geometry/categories/offRoad) survive the save round trip", to_string(mismatchedSlotRooms) + " rooms differ");
		run.Metric("room furniture pieces", static_cast<double>(total), "");
		run.Check(missingBuildings == 0, "every building comes back for the furniture check");
		run.Check(mismatchedRooms == 0, "room furniture survives the save round trip", to_string(mismatchedRooms) + " rooms differ");
		if (total == 0) run.Note("no furnished rooms in this seed's world (small map without residences?), round trip not exercised");
	}

	// 地上的物品存读一致：房间里的普通物品和固定物品(Room::assets)、室外的物品(Player::worldAssets)连同地面摆位、
	// 选项、scriptStarted都要原样回来。读地图段不会重建房间物品(读档不Layout)，物品全靠player段恢复。
	void CheckGroundAssetRoundTrip(TestWorld& world) {
		TestRun& run = TestRun::Get();
		// 造一点"用过"的状态：给一件固定物品加选项并标成脚本已启动；往室外放一件物品。检查完再还原。
		Asset* touched = nullptr;
		for (const auto& entry : world.map->GetBuildings()) {
			for (Room* room : entry.second->GetRooms()) {
				for (const auto& [name, asset] : room->GetAssets()) if (asset->IsFixture() && !touched) touched = asset;
			}
		}
		const bool touchedStarted = touched ? touched->IsScriptStarted() : false;
		if (touched) { touched->AddOption("coretest option"); touched->SetScriptStarted(true); }
		Asset* outdoor = world.player->CreateAsset("asset_burger");
		AssetGround outdoorGround;
		outdoorGround.x = 12.5f; outdoorGround.y = 34.25f; outdoorGround.z = 0.75f;
		outdoorGround.pitch = 5.f; outdoorGround.yaw = -120.f; outdoorGround.roll = 15.f;
		if (outdoor && !world.player->AddWorldAsset(outdoor, outdoorGround)) { world.player->DestroyAsset(outdoor); outdoor = nullptr; }

		SaveArchive archive;
		MapSaveCodec::Save(*world.map, archive);
		PlayerSaveCodec::Save(*world.player, *world.map, archive);
		string error;
		Map* loaded = MapSaveCodec::Load(archive, error);
		run.Check(loaded != nullptr, "map loads back for the ground asset check", error);
		Player* player2 = nullptr;
		if (loaded) {
			size_t before = 0;
			for (const auto& entry : loaded->GetBuildings()) for (Room* room : entry.second->GetRooms()) before += room->GetAssets().size();
			run.Check(before == 0, "loading the map section alone does not re-create room assets", to_string(before));
			player2 = PlayerSaveCodec::Load(archive, *loaded, error);
			run.Check(player2 != nullptr, "player section loads back with the room assets", error);
		}
		if (loaded && player2) {
			auto sameGround = [](const AssetGround& a, const AssetGround& b) {
				return a.x == b.x && a.y == b.y && a.z == b.z && a.pitch == b.pitch && a.yaw == b.yaw && a.roll == b.roll && a.restOnBottom == b.restOnBottom;
			};
			auto sameAsset = [&](const Asset* a, const Asset* b) {
				if (a->GetName() != b->GetName() || a->GetType() != b->GetType() || a->GetCount() != b->GetCount()) return false;
				if (a->IsFixture() != b->IsFixture() || a->GetMobility() != b->GetMobility() || a->GetMeshPath() != b->GetMeshPath()) return false;
				if (!sameGround(a->GetGround(), b->GetGround())) return false;
				if (a->GetOptions() != b->GetOptions() || a->IsScriptStarted() != b->IsScriptStarted()) return false;
				if ((a->GetScript() != nullptr) != (b->GetScript() != nullptr)) return false;
				const AssetFixtureDesc* da = a->GetFixtureDesc();
				const AssetFixtureDesc* db = b->GetFixtureDesc();
				if ((da != nullptr) != (db != nullptr)) return false;
				if (da && (da->type != db->type || da->meshPath != db->meshPath || da->scaleX != db->scaleX || da->scaleY != db->scaleY ||
					da->scaleZ != db->scaleZ || da->scriptModName != db->scriptModName || da->milestoneNames != db->milestoneNames)) return false;
				return true;
			};
			size_t total = 0, fixtures = 0, differing = 0;
			string firstBad;
			for (const auto& entry : world.map->GetBuildings()) {
				auto other = loaded->GetBuildings().find(entry.first);
				if (other == loaded->GetBuildings().end() || other->second->GetRooms().size() != entry.second->GetRooms().size()) { differing++; continue; }
				for (size_t i = 0; i < entry.second->GetRooms().size(); i++) {
					const auto& a = entry.second->GetRooms()[i]->GetAssets();
					const auto& b = other->second->GetRooms()[i]->GetAssets();
					total += a.size();
					bool same = a.size() == b.size();
					for (const auto& [name, asset] : a) {
						if (asset->IsFixture()) fixtures++;
						auto found = b.find(name);
						if (found == b.end() || !sameAsset(asset, found->second)) { same = false; if (firstBad.empty()) firstBad = name; }
					}
					if (!same) differing++;
				}
			}
			run.Metric("room assets saved", static_cast<double>(total), "");
			run.Metric("room fixtures saved", static_cast<double>(fixtures), "");
			run.Check(differing == 0, "room assets (movable and fixtures) survive the save round trip with their ground placement", to_string(differing) + " rooms differ, first: " + firstBad);
			if (touched) {
				Asset* back = loaded->FindAssetByName(touched->GetName());
				run.Check(back && back->IsScriptStarted() && back->GetOptions() == touched->GetOptions(), "a fixture's options and scriptStarted survive the round trip");
			}
			if (outdoor) {
				auto found = player2->GetWorldAssets().find(outdoor->GetName());
				run.Check(found != player2->GetWorldAssets().end() && sameAsset(outdoor, found->second.asset), "an outdoor asset survives the round trip with its ground placement");
			}
			if (total == 0) run.Note("no room assets in this seed's world, round trip not exercised");
		}

		if (touched) { touched->RemoveOption("coretest option"); touched->SetScriptStarted(touchedStarted); }
		if (outdoor) world.player->DestroyAsset(world.player->RemoveWorldAsset(outdoor->GetName()));
	}

	// 瞌睡值(暂定逻辑，只有玩家正在操控的市民扣)：分时段下降、睡眠时长公式、掉到0强制睡16小时、睡着不掉、醒来回满、放手后不扣。
	// 会把世界往前推几十个小时，放在套件最后跑。
	void CheckSleepiness(TestWorld& world) {
		TestRun& run = TestRun::Get();
		auto close = [](double a, double b) { return fabs(a - b) < 1e-6; };
		run.Check(close(Citizen::SleepinessDrop(Time(2100, 1, 1, 10, 0, 0), 3600.0), 0.02) &&
			close(Citizen::SleepinessDrop(Time(2100, 1, 1, 2, 0, 0), 3600.0), 0.03),
			"sleepiness drops 2% per hour by day (6-24) and 3% per hour at night (0-6)");
		run.Check(close(Citizen::SleepinessDrop(Time(2100, 1, 1, 23, 0, 0), 7200.0), 0.05) &&
			close(Citizen::SleepinessDrop(Time(2100, 1, 1, 5, 30, 0), 3600.0), 0.025) &&
			close(Citizen::SleepinessDrop(Time(2100, 1, 1, 0, 0, 0), 86400.0), 0.54),
			"a span that crosses midnight or 6:00 is split between the two rates",
			to_string(Citizen::SleepinessDrop(Time(2100, 1, 1, 0, 0, 0), 86400.0)));
		run.Check(Citizen::SleepHoursFor(1.f) == 0.0 && Citizen::SleepHoursFor(0.9f) == 0.0 && Citizen::SleepHoursFor(0.8f) == 0.0,
			"above 80% sleepiness nobody can sleep (0 hours)");
		run.Check(close(Citizen::SleepHoursFor(0.68f), 7.5) && close(Citizen::SleepHoursFor(0.5f), 12.0) && close(Citizen::SleepHoursFor(0.f), 16.0),
			"sleep hours follow 20-1/(0.0025x): 16 hours awake -> 7.5h, 50% -> 12h, 0% -> 16h",
			to_string(Citizen::SleepHoursFor(0.68f)) + " / " + to_string(Citizen::SleepHoursFor(0.5f)) + " / " + to_string(Citizen::SleepHoursFor(0.f)));

		Citizen* citizen = nullptr;
		for (Citizen* candidate : world.populace->GetCitizens()) if (candidate && candidate->IsActive()) { citizen = candidate; break; }
		if (!citizen) return;
		run.Check(citizen->GetSleepHoursNeeded() == 0.0, "a citizen without dynamic stats never needs sleep");
		citizen->ActivateStats();
		run.Check(close(citizen->GetSleepiness(), 1.0) && !citizen->IsSleeping(), "activating dynamic stats starts fully awake");

		WorldDomains domains;
		domains.map = world.map; domains.populace = world.populace; domains.society = world.society; domains.industry = world.industry;
		domains.traffic = world.traffic; domains.story = world.story; domains.player = world.player; domains.polit = world.polit;
		PostImplement post(world.map, world.populace, world.society, world.story, world.industry, world.traffic, world.player);
		int forcedForControlled = 0;
		string forcedHour;
		WorldStepHooks hooks;
		hooks.controlledCitizen = citizen;
		hooks.onCitizenChanges = [&](Citizen* who, const vector<Change*>& changes) {
			for (Change* change : changes) {
				if (const PlayerSleepChange* sleep = ChangeCast<PlayerSleepChange>(change)) {
					if (who == citizen) { forcedForControlled++; forcedHour = sleep->GetHour(); }
				}
			}
		};
		auto stepOneHour = [&] {
			Time next = *world.player->GetTime();
			next.AddHours(1);
			world.player->SetTime(next);
			WorldStep::TickPopulace(domains, false, post, hooks);
		};

		// 醒着：只降不升，降到0被强制睡下，Core给玩家操控的市民发一条带16小时的player_sleep。
		int hoursAwake = 0;
		bool monotonic = true;
		float previous = citizen->GetSleepiness();
		while (hoursAwake < 80 && !citizen->IsSleeping()) {
			stepOneHour();
			hoursAwake++;
			if (!citizen->IsSleeping() && citizen->GetSleepiness() > previous + 1e-6f) monotonic = false;
			previous = citizen->GetSleepiness();
		}
		run.Metric("hours awake before forced sleep", hoursAwake, "h");
		run.Check(monotonic, "sleepiness only goes down while awake");
		// 从100%掉到0：全按白天算要50小时，全按夜里算要34小时。
		run.Check(citizen->IsSleeping() && hoursAwake >= 34 && hoursAwake <= 50, "sleepiness reaching 0 forces the citizen to sleep", to_string(hoursAwake) + " hours awake");
		run.Check(forcedForControlled == 1 && forcedHour == to_string(Citizen::kForcedSleepHours), "Core sends the controlled citizen one player_sleep of 16 hours",
			to_string(forcedForControlled) + " sent, hour=" + forcedHour);

		// 睡着：不掉瞌睡值、不重复强制；睡满16小时醒来回满。
		bool stayedAsleep = true;
		for (int hour = 0; hour < 15; hour++) {
			stepOneHour();
			if (!citizen->IsSleeping() || citizen->GetSleepiness() > 0.f) stayedAsleep = false;
		}
		run.Check(stayedAsleep && forcedForControlled == 1, "a sleeping citizen stays asleep for the whole 16 hours and is not forced again");
		stepOneHour();
		stepOneHour();
		run.Check(!citizen->IsSleeping() && citizen->GetSleepiness() >= 0.96f, "after the sleep ends the citizen wakes up fully rested", to_string(citizen->GetSleepiness()));

		// 没在操控的市民(操控过又放手的)不扣瞌睡值：停在原处，不会被强制睡觉，也不发player_sleep。
		hooks.controlledCitizen = nullptr;
		const float released = citizen->GetSleepiness();
		for (int hour = 0; hour < 60; hour++) stepOneHour();
		run.Check(citizen->GetSleepiness() == released && !citizen->IsSleeping() && forcedForControlled == 1,
			"a citizen who is not being controlled keeps their sleepiness and is never forced to sleep", to_string(citizen->GetSleepiness()));
		// 重新操控之后接着往下掉。
		hooks.controlledCitizen = citizen;
		stepOneHour();
		run.Check(citizen->GetSleepiness() < released, "sleepiness goes down again once the citizen is controlled again");
	}

	// 房间局部系的约定自洽：局部yaw 0度朝向 = 局部+v方向在世界里的方向(家具"正面朝向+v")。
	void CheckRoomYawConvention(TestWorld& world) {
		TestRun& run = TestRun::Get();
		size_t rooms = 0, bad = 0;
		int badByFace[8] = { 0 }, totalByFace[8] = { 0 };
		for (const auto& entry : world.map->GetBuildings()) {
			for (Room* room : entry.second->GetRooms()) {
				pair<float, float> a = room->LocalToWorld(0.f, 0.f), b = room->LocalToWorld(0.f, 1.f);
				float fx = b.first - a.first, fy = b.second - a.second;
				const float length = sqrtf(fx * fx + fy * fy);
				if (length < 1e-6f) continue;
				const float yaw = room->LocalYawToWorld(0.f);
				const float dot = (fx * cosf(yaw) + fy * sinf(yaw)) / length;
				const int face = static_cast<int>(room->GetDirection()) & 7;
				rooms++; totalByFace[face]++;
				if (dot < 0.99f) { bad++; badByFace[face]++; }
			}
		}
		string detail;
		for (int i = 0; i < 8; i++) if (totalByFace[i]) detail += " face" + to_string(i) + ":" + to_string(badByFace[i]) + "/" + to_string(totalByFace[i]);
		run.Check(bad == 0, "room local yaw 0 faces local +v in world space", to_string(bad) + "/" + to_string(rooms) + " rooms off;" + detail);
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

	// 政治域：先举行一次选举让执政党/权力/政策值有内容，再存-读-存，字节必须一致，关键字段读回相同。
	void CheckPolitRoundTrip(TestWorld& world) {
		TestRun& run = TestRun::Get();
		if (!world.polit) return;
		world.polit->HoldElection(*world.player->GetTime());
		SaveArchive first;
		PolitSaveCodec::Save(*world.polit, *world.populace, first);
		string error;
		Polit* restored = PolitSaveCodec::Load(first, *world.map, *world.populace, *world.society, error);
		world.society->BindPolicy(&world.polit->GetPolicy()); // Load把Society绑到了新Polit，绑回原来的
		run.Check(restored != nullptr, "polit section loads back", error);
		if (!restored) return;
		SaveArchive second;
		PolitSaveCodec::Save(*restored, *world.populace, second);
		run.Check(first.Find(SAVE_SECTION_POLIT)->data == second.Find(SAVE_SECTION_POLIT)->data, "polit section round trips byte-identical");
		run.Check(restored->GetParties().size() == world.polit->GetParties().size(), "party count survives the round trip");
		bool same = true;
		for (const Party* party : world.polit->GetParties()) {
			const Party* other = restored->FindParty(party->GetId());
			if (!other || other->members != party->members || other->leader != party->leader || other->terms != party->terms ||
				other->ruling != party->ruling || other->meetingAddress != party->meetingAddress || other->powers != party->powers) same = false;
		}
		run.Check(same, "party members/leader/terms/ruling/meeting room/powers survive the round trip");
		const GovernmentPolicy& a = world.polit->GetPolicy();
		const GovernmentPolicy& b = restored->GetPolicy();
		run.Check(a.corporateTaxPercent == b.corporateTaxPercent && a.incomeTaxPercent == b.incomeTaxPercent &&
			a.pension == b.pension && a.immigrationTargetRatio == b.immigrationTargetRatio, "policy values survive the round trip");
		run.Check(world.society->GetPolicy().corporateTaxPercent == a.corporateTaxPercent, "society reads the policy owned by the polit domain");
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

	RecordMemory("save: before archive");
	const SaveWorld saveWorld = ToSaveWorld(world);
	const SaveArchive first = SaveGame::BuildArchive(saveWorld);
	RecordMemory("save: after one archive"); // 一份存档(各段字节)的内存开销
	const SaveArchive second = SaveGame::BuildArchive(saveWorld);
	const string different = FirstDifferentSection(first, second);
	run.Check(different.empty(), "saving twice without changes gives identical sections", different);

	CheckArchiveContainer(first);
	CheckRandomRestore(world);
	CheckPopulaceRoundTrip(world);
	CheckPolitRoundTrip(world);
	CheckFurnitureRoundTrip(world);
	CheckGroundAssetRoundTrip(world);
	CheckRoomYawConvention(world);
	CheckSocietyRoundTrip(world);
	RecordMemory("save: after round trips"); // 回环重建的域如果没释放会在这里涨
	CheckSleepiness(world); // 会推进世界时间，放最后
}

void RunSaveSuite(const TestOptions& options) {
	TestRun::Get().Begin("save", 0, options.selfCommand + " --suite save");
	RunSuiteInChildren(options, "save", options.EffectiveMapSize());
}
