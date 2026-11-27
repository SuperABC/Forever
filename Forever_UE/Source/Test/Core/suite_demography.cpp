#include "suites.h"

#include "process_util.h"
#include "test_framework.h"
#include "world_build.h"

#include "common/random.h"
#include "common/utility.h"
#include "common/world_step.h"
#include "map/building.h"
#include "map/map.h"
#include "map/room.h"
#include "player/player.h"
#include "populace/citizen.h"
#include "populace/demography.h"
#include "populace/experience.h"
#include "populace/household.h"
#include "populace/kinship.h"
#include "populace/populace.h"
#include "society/job.h"
#include "society/ledger.h"
#include "society/society.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

// 运行时人口推进(Core/populace/demography)：
//   1) 用DemographyConfig把各项概率拉到1/0，逐个分支确定性地检查：自然死亡和遗产、怀孕、出生(住处/亲属/钱包)、
//      配对结婚同居(异性/成年/单身/非近亲、搬家后旧房空出)、成年搬出、被操控的市民跳过；
//   2) 用默认参数按WorldStep逐日跑400天：账本每天为0、住户和家互相一致、没有作用域外的随机数；
//   3) 事件日志写到 $CORETEST_DEMOGRAPHY_DIR/demography_seed<S>_<run>.log，父进程同种子跑两遍逐字节比对。

namespace {
	string EnvOr(const char* name, const string& fallback) {
		char* value = nullptr;
		size_t size = 0;
		if (_dupenv_s(&value, &size, name) != 0 || !value) return fallback;
		string result(value);
		free(value);
		return result;
	}

	// 只开某一项的配置(其余概率0)，各场景互不干扰。
	DemographyConfig Quiet() {
		DemographyConfig config;
		config.deathStartAge = 100000;
		config.pregnancyChancePerDay = 0.0;
		config.datingScale = 0.0;
		return config;
	}

	vector<Citizen*> Residents(const Room* room) {
		vector<Citizen*> out;
		if (!room) return out;
		for (const Household* household : room->GetHouseholds()) {
			for (Citizen* member : household->GetMembers()) if (member && member->IsActive()) out.push_back(member);
		}
		return out;
	}

	bool HasKinType(const Citizen* a, const Citizen* b, RELATIVE_TYPE type) {
		for (Experience* experience : a->GetExperiences()) {
			if (experience->GetCategory() != RELATIONSHIP_KINSHIP) continue;
			const KinshipExperience* kin = static_cast<KinshipExperience*>(experience);
			if (kin->GetOther() == b && kin->GetRelativeType() == type) return true;
		}
		return false;
	}

	// 住户和家互相一致：活人的家里有他所在的户，户里的成员都把这里当家；没有活人住的住宅没有户。
	string CheckResidency(TestWorld& world) {
		for (Citizen* citizen : world.populace->GetCitizens()) {
			if (!citizen->IsActive() || !citizen->GetRoom()) continue;
			Household* household = citizen->GetHousehold();
			if (!household || household->GetRoom() != citizen->GetRoom() || !household->Contains(citizen)) {
				return citizen->GetName() + " is not in a household of his home";
			}
		}
		for (const auto& [name, building] : world.map->GetBuildings()) {
			if (!building) continue;
			for (Room* room : building->GetRooms()) {
				if (!room) continue;
				for (const Household* household : room->GetHouseholds()) {
					if (household->GetMembers().empty()) return room->GetAddress() + " keeps an empty household";
					for (const Citizen* member : household->GetMembers()) {
						if (member->IsActive() && member->GetRoom() != room) return member->GetName() + " lives elsewhere but is still a member at " + room->GetAddress();
					}
				}
			}
		}
		return "";
	}

	void CheckDeath(TestWorld& world, const Time& now) {
		TestRun& run = TestRun::Get();
		Society& society = *world.society;
		Ledger& ledger = society.GetLedger();

		// 有在世配偶的人：遗产给配偶、工作空出来。
		Citizen* married = nullptr;
		for (Citizen* citizen : world.populace->GetCitizens()) {
			if (citizen->IsActive() && citizen->HasLivingSpouse() && citizen->GetJob()) { married = citizen; break; }
		}
		if (married) {
			Citizen* spouse = married->GetSpouse();
			Job* job = married->GetJob();
			const int64_t before = ledger.Balance(Holder::Of(spouse));
			const int64_t own = ledger.Balance(Holder::Of(married));
			vector<Citizen*> heirs = Demography::Kill(*world.map, society, married);
			run.Check(!married->IsActive() && married->IsDead(), "a natural death marks the citizen dead");
			run.Check(job->GetOccupant() == nullptr && married->GetJob() == nullptr, "the dead citizen's job is released");
			run.Check(heirs.size() == 1 && heirs[0] == spouse, "the living spouse is the only heir");
			run.Check(ledger.Balance(Holder::Of(married)) == 0, "the dead citizen's account ends at zero");
			if (own > 0) run.Check(ledger.Balance(Holder::Of(spouse)) >= before + own, "the spouse receives the estate",
				to_string(before) + " + " + to_string(own) + " -> " + to_string(ledger.Balance(Holder::Of(spouse))));
			run.Check(ledger.Total() == 0, "ledger total stays zero after an inheritance");
			run.Check(married->GetHousehold() == nullptr, "the dead citizen leaves his household");
		}
		else run.Note("no married citizen with a job, inheritance not exercised");

		// 独居的人死了，房子变成空房。
		Citizen* alone = nullptr;
		for (Citizen* citizen : world.populace->GetCitizens()) {
			if (citizen->IsActive() && citizen->GetRoom() && Residents(citizen->GetRoom()).size() == 1) { alone = citizen; break; }
		}
		if (alone) {
			Room* home = alone->GetRoom();
			Demography::Kill(*world.map, society, alone);
			run.Check(home->GetHouseholds().empty(), "the home of a citizen who lived alone becomes vacant after his death");
		}
		else run.Note("nobody lives alone, vacancy after death not exercised");

		// Daily：只让最老的人达到"必死"的概率；他被操控时跳过，不操控时死亡。
		Citizen* oldest = nullptr;
		for (Citizen* citizen : world.populace->GetCitizens()) {
			if (citizen->IsActive() && (!oldest || citizen->GetAge(now) > oldest->GetAge(now))) oldest = citizen;
		}
		if (oldest) {
			DemographyConfig config = Quiet();
			config.deathStartAge = oldest->GetAge(now) - 1;
			config.deathRatePerYear = 1.0;
			DemographyReport skipped = Demography::Daily(*world.populace, *world.map, society, now, oldest, config);
			run.Check(oldest->IsActive(), "the controlled citizen never dies naturally");
			DemographyReport died = Demography::Daily(*world.populace, *world.map, society, now, nullptr, config);
			bool reported = false;
			for (const DemographyReport::Death& death : died.deaths) reported |= death.citizen == oldest;
			run.Check(!oldest->IsActive() && reported, "Daily kills a citizen whose death chance is 1 and reports it");
			(void)skipped;
		}
	}

	void CheckBirth(TestWorld& world, const Time& now) {
		TestRun& run = TestRun::Get();
		const Time today(now.GetYear(), now.GetMonth(), now.GetDay());

		// 怀孕：概率拉到1，所有符合条件的女性都怀上，预产期280天后。
		{
			DemographyConfig config = Quiet();
			config.pregnancyChancePerDay = 1.0;
			Demography::Daily(*world.populace, *world.map, *world.society, now, nullptr, config);
			size_t pregnant = 0, bad = 0;
			Time due = today;
			due.AddDays(config.pregnancyDays);
			for (Citizen* citizen : world.populace->GetCitizens()) {
				if (!citizen->IsPregnant()) continue;
				pregnant++;
				const int age = citizen->GetAge(now);
				if (citizen->GetGender() != GENDER_FEMALE || !citizen->HasLivingSpouse() || age < config.fertileMinAge ||
					age > config.fertileMaxAge || !(citizen->GetPregnancyDue() == due)) bad++;
			}
			run.Metric("pregnant after one forced day", static_cast<double>(pregnant), "");
			run.Check(pregnant > 0, "forced pregnancy chance makes some women pregnant");
			run.Check(bad == 0, "only fertile married women get pregnant, due in 280 days", to_string(bad));
			for (Citizen* citizen : world.populace->GetCitizens()) citizen->ClearPregnancy();
		}

		// 出生：预产期到了的母亲生孩子。
		Citizen* mother = nullptr;
		Citizen* apart = nullptr; // 和丈夫不住一起的母亲
		for (Citizen* citizen : world.populace->GetCitizens()) {
			if (!citizen->IsActive() || citizen->GetGender() != GENDER_FEMALE || !citizen->HasLivingSpouse() || !citizen->GetRoom()) continue;
			if (citizen->GetSpouse()->GetRoom() == citizen->GetRoom()) { if (!mother && !citizen->GetChildren().empty()) mother = citizen; }
			else if (!apart) apart = citizen;
		}
		if (!mother) {
			run.Note("no married mother living with her husband, birth not exercised");
			return;
		}
		vector<Citizen*> siblings = mother->GetChildren();
		Citizen* father = mother->GetSpouse();
		mother->SetPregnancyDue(today);
		if (apart) apart->SetPregnancyDue(today);
		const size_t before = world.populace->GetCitizens().size();
		DemographyReport report = Demography::Daily(*world.populace, *world.map, *world.society, now, nullptr, Quiet());
		Citizen* child = nullptr;
		Citizen* apartChild = nullptr;
		for (const DemographyReport::Birth& birth : report.births) {
			if (birth.mother == mother) child = birth.child;
			if (birth.mother == apart) apartChild = birth.child;
		}
		run.Check(child != nullptr && world.populace->GetCitizens().size() > before, "a mother due today gives birth");
		if (!child) return;
		run.Check(child->GetAge(now) == 0 && child->IsActive(), "the newborn is age 0 and active");
		run.Check(child->GetRoom() == mother->GetRoom() && Residents(mother->GetRoom()).size() >= 3,
			"the newborn lives in the parents' shared home");
		run.Check(find(mother->GetChildren().begin(), mother->GetChildren().end(), child) != mother->GetChildren().end() &&
			find(father->GetChildren().begin(), father->GetChildren().end(), child) != father->GetChildren().end(),
			"both parents list the newborn as a child");
		run.Check(HasKinType(mother, child, RELATIVE_CHILD) && HasKinType(child, mother, RELATIVE_PARENT) &&
			HasKinType(child, father, RELATIVE_PARENT), "kinship between the newborn and the parents is written");
		bool siblingsLinked = true;
		for (Citizen* sibling : siblings) siblingsLinked &= HasKinType(child, sibling, RELATIVE_SIBLING) && HasKinType(sibling, child, RELATIVE_SIBLING);
		run.Check(siblingsLinked, "the newborn and the existing children are siblings");
		run.Check(world.society->GetLedger().Balance(Holder::Of(child)) == 0, "the newborn's wallet starts at zero");
		run.Check(world.populace->FindCitizenByName(child->GetName()) == child, "the newborn's name is unique");
		run.Check(!mother->IsPregnant() && mother->GetLastBirth() == today, "the mother is no longer pregnant and remembers the birth day");
		if (apart) run.Check(apartChild && apartChild->GetRoom() == apart->GetRoom(), "parents living apart: the newborn lives with the mother");
		else run.Note("no married couple living apart, mother's-home rule not exercised");
	}

	void CheckPairing(TestWorld& world, const Time& now) {
		TestRun& run = TestRun::Get();
		Citizen* protectedSingle = nullptr;
		size_t singlesBefore = 0;
		for (Citizen* citizen : world.populace->GetCitizens()) {
			if (citizen->IsActive() && !citizen->HasLivingSpouse() && citizen->GetAge(now) >= 18) {
				singlesBefore++;
				if (!protectedSingle) protectedSingle = citizen;
			}
		}
		DemographyConfig config = Quiet();
		config.datingScale = 100.0; // 每个单身的人都去找对象
		// 搬家前各自的家里有几个活人，用来检查"原来独居的搬走后房子空出来"。
		DemographyReport report = Demography::Daily(*world.populace, *world.map, *world.society, now, protectedSingle, config);
		run.Metric("marriages on one forced day", static_cast<double>(report.marriages.size()), "");
		run.Check(!report.marriages.empty() || singlesBefore < 2, "forced dating chance marries some singles");
		size_t bad = 0, apart = 0;
		string firstBad;
		for (const auto& [a, b] : report.marriages) {
			const bool ok = a->GetGender() != b->GetGender() && a->GetAge(now) >= 18 && b->GetAge(now) >= 18 &&
				a->GetSpouse() == b && b->GetSpouse() == a && !HasKinType(a, b, RELATIVE_PARENT) && !HasKinType(a, b, RELATIVE_CHILD) &&
				!HasKinType(a, b, RELATIVE_SIBLING) && a != protectedSingle && b != protectedSingle;
			if (!ok) { bad++; if (firstBad.empty()) firstBad = a->GetName() + " & " + b->GetName(); }
			if ((a->GetRoom() || b->GetRoom()) && a->GetRoom() != b->GetRoom()) apart++;
		}
		run.Check(bad == 0, "new couples are adult, opposite-sex, unrelated, mutual spouses and never the controlled citizen", firstBad);
		run.Check(apart == 0, "a new couple lives in one home", to_string(apart));
		run.Check(!protectedSingle || !protectedSingle->HasLivingSpouse(), "the controlled citizen is never paired");
		size_t movesBad = 0;
		for (const DemographyReport::Move& move : report.moves) {
			if (move.citizen->GetRoom() != move.to) movesBad++;
			if (move.from && Residents(move.from).empty() && !move.from->GetHouseholds().empty()) movesBad++;
		}
		run.Check(movesBad == 0, "a mover ends up in the new home and an emptied old home becomes vacant", to_string(movesBad));
	}

	void CheckMoveOut(TestWorld& world) {
		TestRun& run = TestRun::Get();
		// 和别人同住的17岁孩子：把日期设成他18岁生日那天。
		Citizen* teen = nullptr;
		for (Citizen* citizen : world.populace->GetCitizens()) {
			if (!citizen->IsActive() || citizen->HasLivingSpouse() || !citizen->GetRoom()) continue;
			if (Residents(citizen->GetRoom()).size() < 2) continue;
			const Time birthday18(citizen->GetBirthYear() + 18, citizen->GetBirthMonth(), citizen->GetBirthDay(), 0, 1);
			if (citizen->GetAge(birthday18) == 18 && citizen->GetAge(*world.player->GetTime()) < 18) { teen = citizen; break; }
		}
		if (!teen) {
			run.Note("no minor living with family, move-out not exercised");
			return;
		}
		const Time birthday18(teen->GetBirthYear() + 18, teen->GetBirthMonth(), teen->GetBirthDay(), 0, 1);
		Room* home = teen->GetRoom();
		vector<Citizen*> family = Residents(home);
		const bool vacancy = world.map->PickVacantResidence() != nullptr;
		// 先当他被操控：跳过。
		Demography::Daily(*world.populace, *world.map, *world.society, birthday18, teen, Quiet());
		run.Check(teen->GetRoom() == home, "the controlled citizen does not move out");
		DemographyReport report = Demography::Daily(*world.populace, *world.map, *world.society, birthday18, nullptr, Quiet());
		if (vacancy) {
			bool reported = false;
			for (const DemographyReport::Move& move : report.moves) reported |= move.citizen == teen && move.reason == DemographyReport::MoveReason::MoveOut;
			run.Check(teen->GetRoom() != home && teen->GetRoom() && teen->GetRoom()->IsResidence() && reported,
				"a citizen living with family moves to a vacant home on his 18th birthday");
			run.Check(Residents(teen->GetRoom()).size() == 1, "the new home was vacant");
			bool familyStays = true;
			for (Citizen* member : family) if (member != teen && member->IsActive()) familyStays &= member->GetRoom() == home;
			run.Check(familyStays, "the family stays in the old home");
			run.Check(!teen->IsPendingMoveOut(), "a successful move-out clears the pending flag");
		}
		else {
			run.Check(teen->GetRoom() == home && teen->IsPendingMoveOut(), "without a vacant home the move-out is retried later");
		}
		// 已经一个人住的人标了pendingMoveOut：清掉、不搬。
		Citizen* alone = nullptr;
		for (Citizen* citizen : world.populace->GetCitizens()) {
			if (citizen->IsActive() && citizen->GetRoom() && Residents(citizen->GetRoom()).size() == 1) { alone = citizen; break; }
		}
		if (alone) {
			Room* aloneHome = alone->GetRoom();
			alone->SetPendingMoveOut(true);
			Demography::Daily(*world.populace, *world.map, *world.society, *world.player->GetTime(), nullptr, Quiet());
			run.Check(alone->GetRoom() == aloneHome && !alone->IsPendingMoveOut(), "someone already living alone does not move out");
		}
	}

	// 默认参数逐日跑：账本守恒、住户一致、事件写日志(同种子两遍比对)。
	void NaturalRun(TestWorld& world, const string& logPath) {
		TestRun& run = TestRun::Get();
		WorldDomains domains;
		domains.map = world.map;
		domains.populace = world.populace;
		domains.society = world.society;
		domains.industry = world.industry;
		domains.traffic = world.traffic;
		domains.story = world.story;
		domains.player = world.player;
		domains.polit = world.polit;
		WorldStepHooks hooks;
		string log;
		size_t born = 0, died = 0, married = 0, moved = 0;
		hooks.onDemography = [&](const DemographyReport& report) {
			const Time& now = *world.player->GetTime();
			const string day = to_string(now.GetYear()) + "-" + to_string(now.GetMonth()) + "-" + to_string(now.GetDay());
			for (const auto& death : report.deaths) log += day + " died " + death.citizen->GetName() + " " + to_string(death.age) + "\n";
			for (const auto& birth : report.births) log += day + " born " + birth.child->GetName() + " to " + birth.mother->GetName() + "\n";
			for (const auto& [a, b] : report.marriages) log += day + " married " + a->GetName() + " " + b->GetName() + "\n";
			for (const auto& move : report.moves) log += day + " moved " + move.citizen->GetName() + " -> " + (move.to ? move.to->GetAddress() : "") + "\n";
			born += report.births.size(); died += report.deaths.size(); married += report.marriages.size(); moved += report.moves.size();
		};
		const long long unscopedBefore = static_cast<long long>(RandomRegistry::UnscopedDraws());
		int badLedgerDays = 0;
		constexpr int kDays = 400;
		for (int day = 0; day < kDays; day++) {
			Time next = *world.player->GetTime();
			next.SetTime(0, 0, 0, 0);
			next.AddDays(1);
			world.player->SetTime(next);
			WorldStep::Step(domains, world.player->CrossDay(), hooks);
			if (world.society->GetLedger().Total() != 0) badLedgerDays++;
		}
		run.Metric("natural run born", static_cast<double>(born), "");
		run.Metric("natural run died", static_cast<double>(died), "");
		run.Metric("natural run marriages", static_cast<double>(married), "");
		run.Metric("natural run moves", static_cast<double>(moved), "");
		run.Check(badLedgerDays == 0, "ledger total stays zero every day of the natural run", to_string(badLedgerDays));
		run.Check(born > 0 && died > 0 && married > 0, "births, deaths and marriages all happen within 400 days",
			to_string(born) + "/" + to_string(died) + "/" + to_string(married));
		const string residency = CheckResidency(world);
		run.Check(residency.empty(), "homes and households agree after the natural run", residency);
		run.Check(static_cast<long long>(RandomRegistry::UnscopedDraws()) == unscopedBefore, "the population step draws no unscoped random numbers");
		if (!logPath.empty()) {
			ofstream out(logPath, ios::binary);
			out << log;
		}
	}
}

void DemographySuiteBody(TestWorld& world, const TestOptions& options) {
	TestRun& run = TestRun::Get();
	const string residency = CheckResidency(world);
	run.Check(residency.empty(), "homes and households agree right after generation", residency);
	const Time now = *world.player->GetTime();
	CheckDeath(world, now);
	CheckBirth(world, now);
	CheckPairing(world, now);
	CheckMoveOut(world);
	const string afterScenarios = CheckResidency(world);
	run.Check(afterScenarios.empty(), "homes and households agree after the forced scenarios", afterScenarios);
	run.Check(world.society->GetLedger().Total() == 0, "ledger total is zero after the forced scenarios");

	const string dir = EnvOr("CORETEST_DEMOGRAPHY_DIR", "");
	const string tag = EnvOr("CORETEST_DEMOGRAPHY_RUN", "a");
	NaturalRun(world, dir.empty() ? string() : dir + "/demography_seed" + to_string(options.childSeed) + "_" + tag + ".log");
}

void RunDemographySuite(const TestOptions& options) {
	TestRun& run = TestRun::Get();
	run.Begin("demography", 0, options.selfCommand + " --suite demography");
	const string dir = options.outDir + "/demography_logs";
	error_code ec;
	filesystem::remove_all(dir, ec);
	filesystem::create_directories(dir, ec);
	_putenv_s("CORETEST_DEMOGRAPHY_DIR", dir.c_str());
	_putenv_s("CORETEST_DEMOGRAPHY_RUN", "a");
	RunSuiteInChildren(options, "demography", options.EffectiveMapSize());
	_putenv_s("CORETEST_DEMOGRAPHY_RUN", "b");
	RunSuiteInChildren(options, "demography", options.EffectiveMapSize());
	for (unsigned long long seed : options.EffectiveSeeds()) {
		const string a = ReadWholeFile(dir + "/demography_seed" + to_string(seed) + "_a.log");
		const string b = ReadWholeFile(dir + "/demography_seed" + to_string(seed) + "_b.log");
		run.Check(!a.empty() && a == b, "same seed gives a byte-identical population event log [seed " + to_string(seed) + "]",
			to_string(a.size()) + " vs " + to_string(b.size()) + " bytes");
	}
}
