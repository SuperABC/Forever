#include "suites.h"

#include "econ_report.h"

#include "process_util.h"
#include "test_framework.h"
#include "world_build.h"

#include "common/world_step.h"
#include "common/utility.h"
#include "common/random.h"
#include "society/job.h"
#include "map/map.h"
#include "populace/populace.h"
#include "populace/citizen.h"
#include "society/society.h"
#include "society/organization.h"
#include "society/economy.h"
#include "industry/industry.h"
#include "player/player.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unordered_set>
#include <string>

using namespace std;

// 每日经济长跑：用WorldStep(和线上StepWorld同一份)逐日推进，逐日校验账本守恒，并给出逐日摘要哈希。
// 摘要哈希写到 $CORETEST_ECON_DIR/econ_digest_seed<S>_<run>.txt，父进程用同种子跑两次比对(严格一致)。

namespace {
	struct Digest {
		unsigned long long hash = 1469598103934665603ULL;
		void Mix(long long value) { hash = Fnv1a(&value, sizeof(value), hash); }
	};

	string EnvOr(const char* name, const string& fallback) {
		char* value = nullptr;
		size_t size = 0;
		if (_dupenv_s(&value, &size, name) != 0 || !value) return fallback;
		string result(value);
		free(value);
		return result;
	}

	long long CountEmployed(const Populace& populace) {
		long long employed = 0;
		for (Citizen* citizen : populace.GetCitizens()) if (citizen->GetJob()) employed++;
		return employed;
	}
}

namespace {
	// 跳到下个月1号再跑一天(月度结算只在月初)。
	DailyReport RunMonthStartDay(TestWorld& world, const WorldDomains& domains) {
		// 逐日推进到下个月1号(CrossDay只认单日跨越)，返回月初那天的日报。
		DailyReport report;
		for (int guard = 0; guard < 40; guard++) {
			Time next = *world.player->GetTime();
			next.SetTime(0, 0, 0, 0);
			next.AddDays(1);
			world.player->SetTime(next);
			const bool crossed = world.player->CrossDay();
			report = WorldStep::Step(domains, crossed, WorldStepHooks());
			if (next.GetDay() == 1) break;
		}
		return report;
	}

	// 从EconSim搬来的场景：分红路径、倒闭->股东集资重启。每个场景后账本总和仍必须为0。
	void RunEconScenarios(TestWorld& world, const WorldDomains& domains) {
		TestRun& run = TestRun::Get();
		Ledger& ledger = world.society->GetLedger();

		// 分红：给第一个有股东的组织注入1万元利润，月初应先缴企业税(25%)再分红，股东再缴所得税。
		for (Organization* organization : world.society->GetOrganizations()) {
			if (organization->GetShareholders().empty()) continue;
			const int64_t gap = organization->GetMod()->initialFunds - ledger.Balance(Holder::Of(organization));
			const int64_t profit = 1000000;
			ledger.Transfer(Holder::Outside(), Holder::Of(organization), max<int64_t>(0, gap) + profit, Flow::INJECTION);
			const DailyReport report = RunMonthStartDay(world, domains);
			run.Check(report.month.settled, "dividend scenario: month settlement ran on the first day of the month");
			run.Check(report.month.corporateTax >= profit * 25 / 100, "dividend scenario: corporate tax is at least 25% of the injected profit",
				to_string(static_cast<long long>(report.month.corporateTax)));
			run.Check(report.month.dividends > 0, "dividend scenario: dividends were paid");
			run.Check(report.month.dividendIncomeTax > 0, "dividend scenario: shareholders paid income tax on dividends");
			run.Check(ledger.Total() == 0, "dividend scenario: ledger total stays zero");
			break;
		}

		// 倒闭->重启：先让12个成年人富起来(重启要每个股东位都出得起初始资金)，再把一个组织打成负资产。
		{
			Organization* victim = world.society->GetOrganizations().front();
			int enriched = 0;
			for (Citizen* citizen : world.populace->GetCitizens()) {
				if (enriched >= 12) break;
				if (citizen->GetAge(world.populace->GetCurrentYear()) >= 18 && !citizen->IsDead()) {
					ledger.Transfer(Holder::Outside(), Holder::Of(citizen), 3000000, Flow::INJECTION);
					enriched++;
				}
			}
			ledger.Transfer(Holder::Of(victim), Holder::Outside(), ledger.Balance(Holder::Of(victim)) + 5000000, Flow::INJECTION);
			run.Check(ledger.Balance(Holder::Of(victim)) < 0, "bankruptcy scenario: organization was driven into negative equity");
			Time next = *world.player->GetTime();
			next.SetTime(0, 0, 0, 0);
			next.AddDays(1);
			world.player->SetTime(next);
			const bool crossed = world.player->CrossDay();
			const DailyReport report = WorldStep::Step(domains, crossed, WorldStepHooks());
			run.Check(report.newBankruptcies >= 1, "bankruptcy scenario: the next day detects the bankruptcy",
				to_string(report.newBankruptcies));
			run.Check(report.restarts >= 1 || victim->IsBankrupt(), "bankruptcy scenario: organization restarts or stays flagged bankrupt");
			run.Check(ledger.Total() == 0, "bankruptcy scenario: ledger total stays zero");
		}
	}
}

void EconSuiteBody(TestWorld& world, const TestOptions& options) {
	TestRun& run = TestRun::Get();
	const int months = options.EffectiveMonths();
	const int days = months * 30;

	run.Check(world.society->GetLedger().Total() == 0, "ledger total is zero after InitFunds",
		to_string(static_cast<long long>(world.society->GetLedger().Total())));

	WorldDomains domains;
	domains.map = world.map;
	domains.populace = world.populace;
	domains.society = world.society;
	domains.industry = world.industry;
	domains.traffic = world.traffic;
	domains.story = world.story;
	domains.player = world.player;
	domains.polit = world.polit;
	WorldStepHooks hooks; // 不注入：测试里脚本Change被忽略
	// 本次模拟中迁入的市民(月报表里统计"迁入后又离城"用)。
	unordered_set<const Citizen*> immigrantSet;
	Reporter::FinanceTotals finance;
	Reporter::ServiceTotals totals;
	hooks.onImmigrated = [&](Citizen* arrived) { immigrantSet.insert(arrived); finance.immigrated++; };

	// 月度经济报表(EconSim的Reporter)：只在父进程第一遍("a")写，同一份摘要比对不受影响。
	unique_ptr<Reporter> reporter;
	const string reportDir = EnvOr("CORETEST_ECON_DIR", "");
	if (!reportDir.empty() && EnvOr("CORETEST_ECON_RUN", "a") == "a") {
		filesystem::create_directories(reportDir);
		reporter = make_unique<Reporter>(*world.map, *world.populace, *world.society, *world.industry,
			reportDir + "/econ_report_seed" + to_string(options.childSeed) + ".txt");
		reporter->PrintCity();
		reporter->PrintInitial();
	}
	const int startYear = world.player->GetTime()->GetYear();

	Digest digest;
	int badLedgerDays = 0, ranDays = 0;
	long long departedTotal = 0, immigrantTotal = 0;
	size_t startPopulation = world.populace->GetCitizens().size();
	double slowestDay = 0;
	long long unscopedBefore = static_cast<long long>(RandomRegistry::UnscopedDraws());

	for (int day = 1; day <= days; day++) {
		if (reporter && (day - 1) % 30 == 0) { reporter->BeginPeriod(); totals = Reporter::ServiceTotals(); finance = Reporter::FinanceTotals(); }
		// 前3天按小时步进(和跳时间一致，覆盖跨小时计时器)，之后每天一步到次日0点。
		const int steps = day <= 3 ? 24 : 1;
		const auto begin = chrono::steady_clock::now();
		for (int step = 0; step < steps; step++) {
			Time next = *world.player->GetTime();
			if (steps == 24) next.AddHours(1);
			else { next.SetTime(0, 0, 0, 0); next.AddDays(1); }
			world.player->SetTime(next);
			const bool crossed = world.player->CrossDay();
			bool economyRan = false;
			const DailyReport report = WorldStep::Step(domains, crossed, hooks, &economyRan);
			if (economyRan) {
				ranDays++;
				departedTotal += static_cast<long long>(report.departed.size());
				if (reporter) {
					reporter->AccumulateIndustryDay();
					if (report.services.count("food")) {
						const ServiceStat& stat = report.services.at("food");
						totals.demand += stat.demand; totals.sold += stat.sold; totals.unmet += stat.unmet; totals.revenue += stat.revenue;
						for (const auto& [quality, sold] : stat.soldByQuality) totals.soldByQuality[quality] += sold;
					}
					if (report.services.count("video")) {
						const ServiceStat& stat = report.services.at("video");
						totals.videoDemand += stat.demand; totals.videoSold += stat.sold; totals.videoUnmet += stat.unmet; totals.videoRevenue += stat.revenue;
					}
					finance.turnover += report.stock.turnover; finance.volume += report.stock.volume;
					finance.bondIssued += report.bondIssued; finance.bondRepaid += report.bondRepaid; finance.printed += report.moneyPrinted;
					finance.deposited += report.invest.deposited; finance.withdrawn += report.invest.withdrawn;
					finance.depositInterest += report.month.interest.deposit; finance.bondInterest += report.month.interest.bond; finance.interestTax += report.month.interest.tax;
					finance.departed += static_cast<int>(report.departed.size());
					finance.departedHouseholds += report.month.departedHouseholds;
					finance.sharesIssued += report.month.sharesIssued; finance.sharesSubscribed += report.month.sharesSubscribed; finance.issueProceeds += report.month.issueProceeds;
					for (Citizen* gone : report.departed) if (immigrantSet.count(gone)) finance.departedImmigrants++;
					totals.newBankruptcies += report.newBankruptcies; totals.restarts += report.restarts; totals.hired += report.hired;
					totals.dismissedByBankruptcy += report.dismissedByBankruptcy; totals.positionsReopened += report.positionsReopened;
					totals.positionsOpened += report.month.positionsOpened; totals.positionsClosed += report.month.positionsClosed; totals.dismissed += report.month.dismissed;
				}
			}
		}
		slowestDay = max(slowestDay, chrono::duration<double>(chrono::steady_clock::now() - begin).count());

		const Ledger& ledger = world.society->GetLedger();
		if (ledger.Total() != 0) badLedgerDays++;
		digest.Mix(static_cast<long long>(ledger.CitizensTotal()));
		digest.Mix(static_cast<long long>(ledger.OrganizationsTotal()));
		digest.Mix(static_cast<long long>(world.populace->GetCitizens().size()));
		digest.Mix(CountEmployed(*world.populace));
		digest.Mix(static_cast<long long>(world.society->GetOrganizations().size()));

		if (reporter && day % 30 == 0) {
			const int month = day / 30;
			const string title = to_string(startYear + (month - 1) / 12) + "年 第" + to_string(month) + "个月";
			reporter->PrintPeriod(title);
			reporter->PrintRunSummary(title, totals);
			int immigrantsActive = 0;
			for (const Citizen* c : immigrantSet) if (c->IsActive()) immigrantsActive++;
			reporter->PrintFinance(title, "月末" + to_string(month), finance, immigrantsActive);
			reporter->RecordSummaryRow("月末" + to_string(month));
		}
		if (day % 30 == 0) {
			const string prefix = "month " + to_string(day / 30) + " ";
			run.Metric(prefix + "citizens", static_cast<double>(world.populace->GetCitizens().size()), "");
			run.Metric(prefix + "employed", static_cast<double>(CountEmployed(*world.populace)), "");
			run.Metric(prefix + "citizens funds", static_cast<double>(ledger.CitizensTotal()), "");
			run.Metric(prefix + "organizations funds", static_cast<double>(ledger.OrganizationsTotal()), "");
		}
	}

	if (reporter) { reporter->PrintSummaryTable(); reporter->PrintFinanceTable(); }

	const size_t endPopulation = world.populace->GetCitizens().size();
	immigrantTotal = static_cast<long long>(endPopulation) - static_cast<long long>(startPopulation) + departedTotal;
	run.Metric("simulated days", days, "");
	run.Metric("economy days", ranDays, "");
	run.Metric("slowest day", slowestDay, "s");
	run.Metric("citizens departed", static_cast<double>(departedTotal), "");
	run.Metric("citizens immigrated (derived)", static_cast<double>(immigrantTotal), "");
	run.Check(badLedgerDays == 0, "ledger total stays zero every day", to_string(badLedgerDays) + " bad days");
	run.Check(ranDays >= days, "economy ran on every day", to_string(ranDays) + "/" + to_string(days));
	run.Check(endPopulation > 0, "population never dies out");

	// 逐人检查：没有NaN/极端资金，岗位互指。
	int badFunds = 0, jobMismatch = 0;
	for (Citizen* citizen : world.populace->GetCitizens()) {
		const double funds = static_cast<double>(world.society->GetLedger().Balance(Holder::Of(citizen)));
		if (std::isnan(funds)) badFunds++;
		if (citizen->GetJob() && citizen->GetJob()->GetOccupant() != citizen) jobMismatch++;
	}
	run.Check(badFunds == 0, "no NaN citizen balance", to_string(badFunds));
	run.Check(jobMismatch == 0, "jobs still point at their holders after the run", to_string(jobMismatch));

	const long long unscopedAfter = static_cast<long long>(RandomRegistry::UnscopedDraws());
	run.Check(unscopedAfter == unscopedBefore, "no unscoped random draw during the daily simulation",
		to_string(unscopedAfter - unscopedBefore) + " draws");

	// 摘要先算完再跑会改资金的场景，场景不进摘要。
	const unsigned long long digestHash = digest.hash;
	RunEconScenarios(world, domains);

	// 摘要写盘供父进程比对同种子两次运行。
	const string dir = EnvOr("CORETEST_ECON_DIR", "");
	if (!dir.empty()) {
		filesystem::create_directories(dir);
		ofstream out(dir + "/econ_digest_seed" + to_string(options.childSeed) + "_" + EnvOr("CORETEST_ECON_RUN", "a") + ".txt");
		out << HexString(digestHash) << "\n";
	}
	run.Note("daily digest " + HexString(digestHash));
}

void RunEconSuite(const TestOptions& options) {
	TestRun::Get().Begin("econ", 0, options.selfCommand + " --suite econ");
	const string dir = options.outDir + "/econ_digests";
	filesystem::remove_all(dir);
	_putenv_s("CORETEST_ECON_DIR", dir.c_str());
	const vector<string> months = { "--months", to_string(options.EffectiveMonths()) };
	_putenv_s("CORETEST_ECON_RUN", "a");
	RunSuiteInChildren(options, "econ", options.EffectiveMapSize(), months);
	_putenv_s("CORETEST_ECON_RUN", "b");
	// 第二遍只为了拿摘要：同种子必须逐日一致。
	TestRun& run = TestRun::Get();
	const int failuresBefore = run.Failures();
	RunSuiteInChildren(options, "econ", options.EffectiveMapSize(), months);
	(void)failuresBefore;
	for (unsigned long long seed : options.EffectiveSeeds()) {
		const string a = ReadWholeFile(dir + "/econ_digest_seed" + to_string(seed) + "_a.txt");
		const string b = ReadWholeFile(dir + "/econ_digest_seed" + to_string(seed) + "_b.txt");
		run.Check(!a.empty() && a == b, "same seed gives identical daily economy digest [seed " + to_string(seed) + "]",
			a + " vs " + b);
	}
	_putenv_s("CORETEST_ECON_DIR", "");
	_putenv_s("CORETEST_ECON_RUN", "");
}
