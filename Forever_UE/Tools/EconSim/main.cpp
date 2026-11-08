#include "map/map.h"
#include "populace/populace.h"
#include "society/society.h"
#include "society/organization.h"
#include "society/settlement.h"
#include "society/job.h"
#include "populace/household.h"
#include "populace/citizen.h"
#include "society/ledger.h"
#include "society/economy.h"
#include "society/immigration.h"
#include "report.h"
#include "common/implement.h"
#include "society/society_save.h"
#include "populace/populace_save.h"
#include "map/map_save.h"
#include "map/building.h"
#include "map/component.h"
#include "map/room.h"
#include "industry/industry.h"
#include "industry/product.h"
#include "industry/storage.h"
#include "industry/industry_save.h"
#include "common/save_archive.h"
#include "common/implement.h"
#include "story/script.h"
#include "common/config.h"
#include "common/registry.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <cstdlib>
#include <map>
#include <unordered_set>
#include <string>


using namespace std;

namespace {
	constexpr int kDefaultMapSize = 1024;

	// 全城某个product的总库存(所有Storage含工坊cache)。
	double TotalStock(Industry& industry, const string& product) {
		double total = 0;
		for (const auto& [name, storage] : industry.GetStorages()) total += storage->GetAmount(product);
		for (const auto& [name, manufacture] : industry.GetManufactures())
			total += manufacture->GetInputCache()->GetAmount(product) + manufacture->GetOutputCache()->GetAmount(product);
		return total;
	}

	// Step 5:逐日跑完整经济链(Industry物流 -> 货款 -> 服务买卖 -> 房租工资补助 -> 月度分红),每天校验总和为0。
	void RunEconomy(Industry& industry, Society& society, Map& city, Populace& populace, int days) {
		printf("[Industry] 仓库 %zu 个, 工坊 %zu 个\n", industry.GetStorages().size(), industry.GetManufactures().size());
		std::vector<Room*> allRooms;
		for (const auto& [name, building] : city.GetBuildings())
			for (Room* room : building->GetRooms()) allRooms.push_back(room);
		PostImplement post(&city, &populace, &society, nullptr, &industry, nullptr, nullptr);

		int64_t totalSold = 0, totalUnmet = 0;
		for (int day = 1; day <= days; day++) {
			// 每月30天,方便跨月。
			Time time(2000, 1 + (day - 1) / 30, 1 + (day - 1) % 30);
			Economy::SyncIndustry(society, industry);
			industry.Tick(time, true, &post);
			DailyReport report = Economy::Daily(society, industry, allRooms, populace.GetCitizens(), time, &post,
				populace.GetCurrentYear());

			long long imported = 0, exported = 0, internal = 0;
			for (const GoodsFlow& flow : industry.GetFlows()) {
				if (flow.IsImport()) imported += flow.quantity;
				else if (flow.IsExport()) exported += flow.quantity;
				else internal += flow.quantity;
			}
			const ServiceStat stat = report.services.count("food") ? report.services.at("food") : ServiceStat();
			totalSold += stat.sold;
			totalUnmet += stat.unmet;
			const Ledger& ledger = society.GetLedger();
			printf("[Day %2d] 货物 进口%lld 出口%lld 内部%lld | 吃饭 需%d 卖%d 没买到%d 营业额%s 消费税%s | 货款 进口%s 关税%s 出口%s\n", day,
				imported, exported, internal, stat.demand, stat.sold, stat.unmet, FormatMoney(stat.revenue).c_str(),
				FormatMoney(stat.consumptionTax).c_str(), FormatMoney(report.goods.importCost).c_str(),
				FormatMoney(report.goods.tariff).c_str(), FormatMoney(report.goods.exportRevenue).c_str());
			printf("[Day %2d] 工资%s(%d人,代扣所得税%s) 房租%s(房租税%s) 补助 养老%s(%d) 失业%s(%d) 未成年%s(%d)\n", day,
				FormatMoney(report.wages).c_str(), report.wagesPaid, FormatMoney(report.wageIncomeTax).c_str(),
				FormatMoney(report.rent.rentPaid).c_str(), FormatMoney(report.rent.rentTax).c_str(),
				FormatMoney(report.pensions).c_str(), report.pensionCount, FormatMoney(report.unemployment).c_str(),
				report.unemploymentCount, FormatMoney(report.minorAllowance).c_str(), report.minorCount);
			if (report.month.settled) {
				printf("[Day %2d] 月度分红 企业税%s 分红%s 股东所得税%s\n", day, FormatMoney(report.month.corporateTax).c_str(),
					FormatMoney(report.month.dividends).c_str(), FormatMoney(report.month.dividendIncomeTax).c_str());
			}
			if (report.newBankruptcies || report.restarts || report.bankruptTotal) {
				printf("[Day %2d] 企业 新倒闭%d 重启%d(集资%s,核销负债%s) 仍倒闭%d\n", day, report.newBankruptcies, report.restarts,
					FormatMoney(report.restartFunds).c_str(), FormatMoney(report.debtWrittenOff).c_str(), report.bankruptTotal);
			}
			printf("[Day %2d] 余额 市民%s 组织%s 政府%s 城市外%s | 总和%s%s\n", day,
				FormatMoney(ledger.CitizensTotal()).c_str(), FormatMoney(ledger.OrganizationsTotal()).c_str(),
				FormatMoney(ledger.Balance(Party::Government())).c_str(), FormatMoney(ledger.Balance(Party::Outside())).c_str(),
				FormatMoney(ledger.Total()).c_str(), ledger.Total() == 0 ? "" : " !!! 总和不为0");
		}
		// 分红路径:给第一个有股东的组织注入一笔利润(使余额超过初始资金1万元),跨月后应先缴企业税再分红。
		for (Organization* organization : society.GetOrganizations()) {
			if (organization->GetShareholders().empty()) continue;
			Ledger& ledger = society.GetLedger();
			int64_t gap = organization->GetMod()->initialFunds - ledger.Balance(Party::Of(organization));
			int64_t profit = 1000000; // 1万元
			ledger.Transfer(Party::Outside(), Party::Of(organization), max<int64_t>(0, gap) + profit, Flow::INJECTION);
			Time nextMonth(2000, 1 + days / 30 + 1, 1);
			industry.Tick(nextMonth, true, &post);
			DailyReport r = Economy::Daily(society, industry, allRooms, populace.GetCitizens(), nextMonth, &post, populace.GetCurrentYear());
			printf("[DividendCheck] 利润%s -> 企业税%s(期望%s) 分红%s(期望%s) 股东所得税%s(期望%s) 总和%s\n",
				FormatMoney(profit).c_str(), FormatMoney(r.month.corporateTax).c_str(), FormatMoney(profit * 25 / 100).c_str(),
				FormatMoney(r.month.dividends).c_str(), FormatMoney(profit - profit * 25 / 100).c_str(),
				FormatMoney(r.month.dividendIncomeTax).c_str(), FormatMoney((profit - profit * 25 / 100) * 20 / 100).c_str(),
				FormatMoney(ledger.Total()).c_str());
			break;
		}
		int testDay = days + 2;
		auto runDay = [&](const std::vector<Citizen*>& citizens) {
			Time time(2000, 1 + (testDay - 1) / 30, 1 + (testDay - 1) % 30);
			testDay++;
			Economy::SyncIndustry(society, industry);
			industry.Tick(time, true, &post);
			return Economy::Daily(society, industry, allRooms, citizens, time, &post, populace.GetCurrentYear());
		};
		// 倒闭 -> 同一天股东集资重启:把一个组织打成负资产,下一天应当倒闭并立刻重启(市民存款足够)。
		{
			Organization* victim = society.GetOrganizations().front();
			Ledger& ledger = society.GetLedger();
			// 重启要求每个股东位都有存款超过"初始资金×占股比例"(2万元)的成年市民,而普通市民只有五千元左右,
			// 所以先让12个成年人富起来(3万元),再验证重启路径。
			int enriched = 0;
			for (Citizen* citizen : populace.GetCitizens()) {
				if (enriched >= 12) break;
				if (citizen->GetAge(populace.GetCurrentYear()) >= 18 && !citizen->IsDead()) {
					ledger.Transfer(Party::Outside(), Party::Of(citizen), 3000000, Flow::INJECTION);
					enriched++;
				}
			}
			ledger.Transfer(Party::Of(victim), Party::Outside(), ledger.Balance(Party::Of(victim)) + 5000000, Flow::INJECTION);
			printf("[BankruptCheck] 打成负资产: 余额%s\n", FormatMoney(ledger.Balance(Party::Of(victim))).c_str());
			DailyReport r = runDay(populace.GetCitizens());
			printf("[BankruptCheck] 新倒闭%d 重启%d 集资%s 核销负债%s | 重启后余额%s(期望%s) 仍倒闭=%d 股东%zu人 总和%s\n",
				r.newBankruptcies, r.restarts, FormatMoney(r.restartFunds).c_str(), FormatMoney(r.debtWrittenOff).c_str(),
				FormatMoney(ledger.Balance(Party::Of(victim))).c_str(), FormatMoney(victim->GetMod()->initialFunds).c_str(),
				victim->IsBankrupt(), victim->GetShareholders().size(), FormatMoney(ledger.Total()).c_str());
		}
		// 倒闭停发工资:没有市民能凑出重启资金(传入空市民列表)时一直倒闭;恢复营业后工资应恢复。
		{
			Organization* target = nullptr;
			int filled = 0;
			for (Organization* organization : society.GetOrganizations()) {
				filled = 0;
				for (Job* job : organization->GetJobs()) if (job->GetOccupant()) filled++;
				if (filled > 0) { target = organization; break; }
			}
			if (target) {
				Ledger& ledger = society.GetLedger();
				// 排除干扰:给所有企业补足资金并恢复营业,避免别的企业在两天之间自然倒闭而影响工资人数。
				for (Organization* organization : society.GetOrganizations()) {
					ledger.Transfer(Party::Outside(), Party::Of(organization), 50000000, Flow::INJECTION);
					organization->SetBankrupt(false);
				}
				target->SetBankrupt(true);
				DailyReport closed = runDay({});
				target->SetBankrupt(false);
				DailyReport open = runDay({});
				printf("[BankruptCheck] 倒闭时发工资%d人 恢复后%d人 差%d人(期望该企业在岗%d人) 总和%s\n", closed.wagesPaid, open.wagesPaid,
					open.wagesPaid - closed.wagesPaid, filled, FormatMoney(ledger.Total()).c_str());
			}
		}

		printf("[Economy] %d天累计:卖出%lld份 没买到%lld份\n", days, static_cast<long long>(totalSold), static_cast<long long>(totalUnmet));
	}

	// 存档往返:存industry -> 读回成新的Industry -> 两边各跑下一天,库存和流水必须完全一致。
	bool CheckIndustrySave(Industry& industry, Map& city, int nextDay) {
		SaveArchive archive;
		IndustrySaveCodec::Save(industry, archive);
		string error;
		Industry* restored = IndustrySaveCodec::Load(archive, city, error);
		if (!restored) {
			printf("[SaveCheck] 读档失败: %s\n", error.c_str());
			return false;
		}

		// 缺段必须报错,不能静默返回。
		SaveArchive empty;
		string missingError;
		Industry* none = IndustrySaveCodec::Load(empty, city, missingError);
		printf("[SaveCheck] 缺工业段: 返回空=%d 有错误信息=%d (%s)\n", none == nullptr, !missingError.empty(), missingError.c_str());
		delete none;

		SaveArchive again;
		IndustrySaveCodec::Save(*restored, again);
		bool sameBytes = archive.Find(SAVE_SECTION_INDUSTRY)->data == again.Find(SAVE_SECTION_INDUSTRY)->data;

		industry.Tick(Time(2000, 1, nextDay), true, nullptr);
		restored->Tick(Time(2000, 1, nextDay), true, nullptr);
		bool sameFlows = industry.GetFlows().size() == restored->GetFlows().size();
		for (size_t i = 0; sameFlows && i < industry.GetFlows().size(); i++) {
			const GoodsFlow& a = industry.GetFlows()[i];
			const GoodsFlow& b = restored->GetFlows()[i];
			sameFlows = a.product == b.product && a.quantity == b.quantity &&
				(a.from ? a.from->GetName() : "") == (b.from ? b.from->GetName() : "") &&
				(a.to ? a.to->GetName() : "") == (b.to ? b.to->GetName() : "");
		}
		SaveArchive afterA, afterB;
		IndustrySaveCodec::Save(industry, afterA);
		IndustrySaveCodec::Save(*restored, afterB);
		bool sameAfter = afterA.Find(SAVE_SECTION_INDUSTRY)->data == afterB.Find(SAVE_SECTION_INDUSTRY)->data;
		printf("[SaveCheck] 往返字节一致=%d 次日流水一致=%d(%zu笔) 次日状态一致=%d 存档段大小=%zu\n", sameBytes, sameFlows,
			industry.GetFlows().size(), sameAfter, archive.Find(SAVE_SECTION_INDUSTRY)->data.size());
		delete restored;
		return sameBytes && sameFlows && sameAfter;
	}

	// Step 4:户、岗位定员、股东、政府拥有比例的统计。
	void ReportStep4(Map& city, Society& society, Populace& populace) {
		// 户
		std::map<size_t, int> householdsPerRoom;
		int residents = 0, multiMember = 0, withoutHousehold = 0;
		int stated = 0, rooms = 0;
		for (const auto& [name, building] : city.GetBuildings()) {
			for (Room* room : building->GetRooms()) {
				rooms++;
				if (room->GetStated()) stated++;
				if (room->IsResidential() && !room->GetHouseholds().empty()) householdsPerRoom[room->GetHouseholds().size()]++;
				for (Household* household : room->GetHouseholds()) {
					residents += static_cast<int>(household->GetMembers().size());
					if (household->GetMembers().size() > 1) multiMember++;
				}
			}
		}
		for (Citizen* citizen : populace.GetCitizens()) if (citizen->GetRoom() && !citizen->GetHousehold()) withoutHousehold++;
		for (const auto& [count, rooms2] : householdsPerRoom) printf("[Step4] 住宅房间里有%zu户: %d间\n", count, rooms2);
		printf("[Step4] 住户共%d人,多人户%d户,有房间却没归户的市民%d人\n", residents, multiMember, withoutHousehold);
		printf("[Step4] 政府拥有的房间 %d/%d (%.1f%%)\n", stated, rooms, rooms ? 100.0 * stated / rooms : 0.0);

		// 岗位与股东
		std::map<string, int> jobsByType, filledByType;
		for (Organization* organization : society.GetOrganizations()) {
			for (Job* job : organization->GetJobs()) {
				jobsByType[job->GetType()]++;
				if (job->GetOccupant()) filledByType[job->GetType()]++;
			}
		}
		for (const auto& [type, count] : jobsByType) printf("[Step4] 岗位 %s: %d (已招%d)\n", type.c_str(), count, filledByType[type]);
		for (Organization* organization : society.GetOrganizations()) {
			int64_t sum = 0;
			for (const auto& s : organization->GetShareholders()) sum += s.shares;
			printf("[Step4] 示例组织 %s 股东%zu人 持股和=%lld/%lld\n", organization->GetType().c_str(), organization->GetShareholders().size(),
			static_cast<long long>(sum), static_cast<long long>(organization->GetTotalShares()));
			break;
		}
	}

	// 户的归并规则:无亲属分户,亲属合户,新住户同时连起两户时合并。用一间空住宅房间和临时市民验证,
	// 放在最后做(会在这间房里留下户)。
	void CheckHouseholdRules(Map& city) {
		Room* room = nullptr;
		for (const auto& [name, building] : city.GetBuildings()) {
			for (Room* candidate : building->GetRooms()) {
				if (candidate->IsResidential() && candidate->GetHouseholds().empty()) { room = candidate; break; }
			}
			if (room) break;
		}
		if (!room) { printf("[HouseholdCheck] 没有空住宅房间,跳过\n"); return; }

		static Citizen a("甲", GENDER_MALE, 1980, 1, 1); static Citizen b("乙", GENDER_FEMALE, 1981, 1, 1);
		static Citizen c("丙", GENDER_FEMALE, 1982, 1, 1); static Citizen e("丁", GENDER_MALE, 2010, 1, 1);
		a.SetSpouse(&c); c.SetSpouse(&a); // 甲丙是夫妻
		a.AddChild(&e); b.AddChild(&e);   // 丁是甲和乙共同的孩子
		auto count = [&]() { return room->GetHouseholds().size(); };
		room->AddResident(&a);
		size_t afterA = count();
		room->AddResident(&b); // 与甲无关系 -> 新的一户
		size_t afterB = count();
		room->AddResident(&c); // 甲的配偶 -> 并入甲那一户
		size_t afterC = count();
		room->AddResident(&e); // 同时是甲和乙的孩子 -> 两户合并
		size_t afterE = count();
		printf("[HouseholdCheck] 甲入住后%zu户(期望1) 乙入住后%zu户(期望2) 丙(甲配偶)入住后%zu户(期望2) 丁(甲乙共同孩子)入住后%zu户(期望1) 成员%zu人(期望4)\n",
			afterA, afterB, afterC, afterE, room->GetHouseholds().empty() ? 0 : room->GetHouseholds()[0]->GetMembers().size());
	}

	bool IsLeapYear(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

	int DaysInMonth(int year, int month) {
		static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
		return month == 2 && IsLeapYear(year) ? 29 : days[month - 1];
	}

	// Step 7:报告模式。开局时间强制设定为人口当前年份的1月1日（保证每次运行可公平对比），按真实日历逐日
	// 跑完整经济链，每月输出政府/个人/组织/外部的收入、支出、结余和分布。
	void RunReport(Map& city, Populace& populace, Society& society, int months, const string& reportPath) {
		Industry industry;
		industry.Init(city.GetAllComponents());
		industry.SeedInitialStock();
		Settlement::InitFunds(society, populace.GetCitizens());

		Reporter reporter(city, populace, society, industry, reportPath);
		int startYear = populace.GetCurrentYear();
		reporter.PrintCity();
		reporter.PrintInitial();
		printf("\n(说明: 开局时间强制设为 %d-01-01; 月度分红在下月1日结算, 计入下一个月的报表。)\n", startYear);

		std::vector<Room*> allRooms;
		for (const auto& [name, building] : city.GetBuildings())
			for (Room* room : building->GetRooms()) allRooms.push_back(room);
		PostImplement post(&city, &populace, &society, nullptr, &industry, nullptr, nullptr);

		std::unordered_set<const Citizen*> immigrantSet; // 本次模拟中迁入的市民
		auto simStart = chrono::steady_clock::now();
		for (int month = 1; month <= months; month++) {
			int year = startYear + (month - 1) / 12;
			int calendarMonth = (month - 1) % 12 + 1;
			reporter.BeginPeriod();
			Reporter::ServiceTotals totals;
			Reporter::FinanceTotals finance;
			for (int day = 1; day <= DaysInMonth(year, calendarMonth); day++) {
				Time time(year, calendarMonth, day);
				// 压力测试：环境变量ECON_DRAIN=每天从政府账户划走的分数，用来把政府打成赤字，验证发国债和印钞。
				char* drain = nullptr;
				size_t drainLength = 0;
				if (_dupenv_s(&drain, &drainLength, "ECON_DRAIN") == 0 && drain) {
					society.GetLedger().Transfer(Party::Government(), Party::Outside(), atoll(drain), Flow::DEBT_WRITE_OFF);
					free(drain);
				}
				Economy::SyncIndustry(society, industry);
				industry.Tick(time, true, &post);
				DailyReport daily = Economy::Daily(society, industry, allRooms, populace.GetCitizens(), time, &post,
					populace.GetCurrentYear());
				reporter.AccumulateIndustryDay();
				if (daily.services.count("food")) {
					const ServiceStat& stat = daily.services.at("food");
					totals.demand += stat.demand;
					totals.sold += stat.sold;
					totals.unmet += stat.unmet;
					totals.revenue += stat.revenue;
				}
				for (Citizen* arrived : Immigration::Daily(society, populace, city, populace.GetCurrentYear())) {
					immigrantSet.insert(arrived);
					finance.immigrated++;
				}
				finance.turnover += daily.stock.turnover;
				finance.volume += daily.stock.volume;
				finance.bondIssued += daily.bondIssued;
				finance.bondRepaid += daily.bondRepaid;
				finance.printed += daily.moneyPrinted;
				finance.deposited += daily.invest.deposited;
				finance.withdrawn += daily.invest.withdrawn;
				finance.depositInterest += daily.month.interest.deposit;
				finance.bondInterest += daily.month.interest.bond;
				finance.interestTax += daily.month.interest.tax;
				finance.departed += static_cast<int>(daily.departed.size());
				finance.departedHouseholds += daily.month.departedHouseholds;
				finance.sharesIssued += daily.month.sharesIssued;
				finance.sharesSubscribed += daily.month.sharesSubscribed;
				finance.issueProceeds += daily.month.issueProceeds;
				for (Citizen* gone : daily.departed) {
					if (immigrantSet.count(gone)) finance.departedImmigrants++;
				}
				totals.newBankruptcies += daily.newBankruptcies;
				totals.restarts += daily.restarts;
				totals.hired += daily.hired;
				totals.dismissedByBankruptcy += daily.dismissedByBankruptcy;
				totals.positionsReopened += daily.positionsReopened;
				totals.positionsOpened += daily.month.positionsOpened;
				totals.positionsClosed += daily.month.positionsClosed;
				totals.dismissed += daily.month.dismissed;
				if (society.GetLedger().Total() != 0) {
					printf("!!! %d-%02d-%02d 全系统资金总和不为0: %s\n", year, calendarMonth, day,
						FormatMoney(society.GetLedger().Total()).c_str());
				}
			}
			char title[64];
			snprintf(title, sizeof(title), "%d年%d月 (第%d个月)", year, calendarMonth, month);
			reporter.PrintPeriod(title);
			reporter.PrintRunSummary(title, totals);
			{
				int immigrantsActive = 0;
				for (const Citizen* c : immigrantSet) if (c->IsActive()) immigrantsActive++;
				char financeLabel[32];
				snprintf(financeLabel, sizeof(financeLabel), "%d月末", calendarMonth);
				reporter.PrintFinance(title, financeLabel, finance, immigrantsActive);
			}
			{
				const Treasury& treasury = society.GetTreasury();
				int activeCount = 0;
				for (const Citizen* c : populace.GetCitizens()) if (c->IsActive()) activeCount++;
				printf("[金融] 股票成交额=%s 存款=%s 国债=%s(本月新发%s) 印钞=%s 政府账户=%s 净头寸=%s 城市外=%s 离城%d人 迁入%d人 在场%d人\n",
					FormatMoney(finance.turnover).c_str(), FormatMoney(treasury.TotalDeposits()).c_str(), FormatMoney(treasury.TotalBonds()).c_str(),
					FormatMoney(finance.bondIssued).c_str(), FormatMoney(finance.printed).c_str(),
					FormatMoney(society.GetLedger().Balance(Party::Government())).c_str(),
					FormatMoney(treasury.NetPosition(society.GetLedger())).c_str(),
					FormatMoney(society.GetLedger().Balance(Party::Outside())).c_str(), finance.departed, finance.immigrated, activeCount);
			}
			char label[32];
			snprintf(label, sizeof(label), "%d月末", calendarMonth);
			reporter.RecordSummaryRow(label);
			fprintf(stderr, "[progress] 第%d个月完成, 已用 %.0fs | 注资比x=%.3f 出口阈值=%.1f%% 税率系数=%.3f 补助系数=%.3f\n", month,
				chrono::duration<double>(chrono::steady_clock::now() - simStart).count(), society.GetLedger().InjectionRatio(),
				100.0 * (1.0 - 1.0 / (society.GetLedger().InjectionRatio() + 1.0)), society.GetPolicy().taxFactor,
				society.GetPolicy().allowanceFactor);
		}
		reporter.PrintSummaryTable();
		reporter.PrintFinanceTable();
	}

	// Step 1自检:价格/仓库权限/房间字段/商店服务收集这几项mod数据改造是否生效。
	void CheckModData(Map& city, Society& society) {
		Industry industry;
		for (const char* id : { "product_wheat", "product_beef", "product_burger" }) {
			Product* product = industry.CreateProduct(id);
			printf("[Check] %s 有效=%d 零售=%lld 进口=%lld 出口=%lld 批发=%lld 服务=%s/%.1f\n", id,
				product != nullptr, product ? (long long)product->GetRetailPrice() : 0,
				product ? (long long)product->GetImportPrice() : 0, product ? (long long)product->GetExportPrice() : 0,
				product ? (long long)product->GetWholesalePrice() : 0,
				product ? product->GetServiceType().c_str() : "-", product ? product->GetServiceQuality() : 0.f);
		}

		Storage* shopStorage = industry.CreateStorage("storage_shop", "check_shop_storage");
		printf("[Check] storage_shop 有效=%d 可取=%d 可放=%d 类型需求数=%zu\n", shopStorage != nullptr,
			shopStorage && shopStorage->CanTake(), shopStorage && shopStorage->CanPut(),
			shopStorage ? shopStorage->GetServiceTypeDemand().size() : 0);

		std::map<string, int> roomCount;
		for (Component* component : city.GetAllComponents()) {
			for (Room* room : component->GetRooms()) {
				string key = room->GetType() + " 工位" + to_string(room->WorkspaceCapacity()) +
					" 租金" + to_string(room->Rent()) + " storageType=" + room->StorageType();
				roomCount[key]++;
			}
		}
		for (const auto& [key, count] : roomCount) printf("[Check] 房间 %s : %d\n", key.c_str(), count);

		// 商店服务收集:往一个商店组织的script里写self.stock.product_burger,经Post查价格。
		PostImplement post(&city, nullptr, &society, nullptr, &industry, nullptr, nullptr);
		for (Organization* organization : society.GetOrganizations()) {
			if (!organization->GetScript()) continue;
			OrganizationMod* mod = organization->GetMod();
			printf("[Check] 组织 %s 初始资金=%lld 股东数=%zu 日薪=%lld\n", mod->GetType(),
				(long long)mod->initialFunds, mod->shareholders.size(), (long long)mod->wage);
			organization->GetScript()->SetValue("stock.product_burger", ValueType(7));
			mod->CollectServices(organization->GetScript(), { "product_burger" }, &post);
			for (const ServiceOffer& s : mod->services)
				printf("[Check] 服务 name=%s type=%s 单价=%lld 质量=%.1f\n", s.name.c_str(), s.type.c_str(),
					(long long)s.unitPrice, s.quality);
			mod->ApplyServiceUsage("product_burger", 3, organization->GetScript());
			printf("[Check] 卖3份后库存=%d\n", ToInt(organization->GetScript()->GetValue("stock.product_burger").second));
			break;
		}
	}
}

static int RealMain(int argc, char** argv);

int main(int argc, char** argv) {
	try {
		return RealMain(argc, argv);
	}
	catch (const std::exception& e) {
		fprintf(stderr, "[EconSim] exception: %s\n", e.what());
		return 2;
	}
}

static int RealMain(int argc, char** argv) {
	string configPath = argc > 1 ? argv[1] : "config.json";
	int mapSize = argc > 2 ? atoi(argv[2]) : kDefaultMapSize;
	int days = argc > 3 ? atoi(argv[3]) : 5;

	// 和AForeverFrameworkActor::BeginPlay同一个编排顺序,只是去掉全部UE可视化步骤。
	Config::ReadConfig(configPath);
	Registry::Get().RefreshMods();

	auto begin = chrono::steady_clock::now();
	Map city(mapSize, mapSize);
	city.InitTerrains();
	city.InitRoadnet();
	city.InitZones();
	city.InitBuildings();

	Populace populace;
	populace.Init(city.ComputeAccommodationTarget(), city.ComputeWorkCapacity());
	city.Checkin(populace);

	Society society;
	society.Init(city.GetAllComponents());
	society.RecruitCitizens(populace.GetCitizens(), populace.GetCurrentYear());
	society.AssignShareholders(populace.GetCitizens(), populace.GetCurrentYear());

	if (argc > 4 && string(argv[4]) == "census") {
		// 人口岗位普查: 市民总数/成年人(>=18)/劳动年龄人口(18~退休年龄前)/岗位总数/在职人数/就业率。
		int year = populace.GetCurrentYear();
		int adults = 0, working = 0, jobs = 0, employed = 0;
		for (Citizen* citizen : populace.GetCitizens()) {
			int age = citizen->GetAge(year);
			if (age >= 18) adults++;
			if (age >= 18 && age < society.GetPolicy().retirementAge) working++;
			if (citizen->GetJob()) employed++;
		}
		for (Organization* organization : society.GetOrganizations()) jobs += (int)organization->GetJobs().size();
		printf("CENSUS %zu %d %d %d %d\n", populace.GetCitizens().size(), adults, working, jobs, employed);
		return 0;
	}

	if (argc > 4 && string(argv[4]) == "report") {
		// 报告模式: 第3个参数是模拟月数, 第5个参数是报表文件路径。
		printf("[EconSim] 城市生成耗时 %.1fs\n", chrono::duration<double>(chrono::steady_clock::now() - begin).count());
		RunReport(city, populace, society, days, argc > 5 ? argv[5] : "");
		return 0;
	}

	std::map<string, int> buildingCount, organizationCount;
	for (const auto& [name, building] : city.GetBuildings()) buildingCount[building->GetType()]++;
	for (Organization* organization : society.GetOrganizations()) organizationCount[organization->GetType()]++;

	printf("[EconSim] 城市生成耗时 %.1fs\n",
		chrono::duration<double>(chrono::steady_clock::now() - begin).count());
	printf("[EconSim] 市民 %zu 人\n", populace.GetCitizens().size());
	for (const auto& [type, count] : buildingCount) printf("[EconSim] 建筑 %s: %d\n", type.c_str(), count);
	for (const auto& [type, count] : organizationCount) printf("[EconSim] 组织 %s: %d\n", type.c_str(), count);
	ReportStep4(city, society, populace);
	CheckModData(city, society);

	Settlement::InitFunds(society, populace.GetCitizens());
	{
		const Ledger& ledger = society.GetLedger();
		printf("[Funds] 市民 %s 组织 %s 政府 %s 城市外 %s 总和 %s\n", FormatMoney(ledger.CitizensTotal()).c_str(),
			FormatMoney(ledger.OrganizationsTotal()).c_str(), FormatMoney(ledger.Balance(Party::Government())).c_str(),
			FormatMoney(ledger.Balance(Party::Outside())).c_str(), FormatMoney(ledger.Total()).c_str());
	}

	Industry industry;
	industry.Init(city.GetAllComponents());
	industry.SeedInitialStock();
	double seededWheat = TotalStock(industry, "product_wheat"), seededBeef = TotalStock(industry, "product_beef");
	printf("[Industry] 开局产出cache库存:小麦%.0f 牛肉%.0f(每个小麦厂5、牛肉厂5)\n", seededWheat, seededBeef);
	RunEconomy(industry, society, city, populace, days);
	CheckIndustrySave(industry, city, days + 1);

	// 人口段(含户)往返:地图+人口存档 -> 读回成新的Map/Populace -> 再存一次,人口段字节必须一致。
	{
		SaveArchive first;
		MapSaveCodec::Save(city, first);
		PopulaceSaveCodec::Save(populace, city, nullptr, first);
		string error;
		Map* map2 = MapSaveCodec::Load(first, error);
		std::vector<std::pair<JobExperience*, int>> pending;
		Populace* populace2 = map2 ? PopulaceSaveCodec::Load(first, *map2, pending, error) : nullptr;
		if (!populace2) {
			printf("[SaveCheck] 人口段读档失败: %s\n", error.c_str());
		}
		else {
			SaveArchive second;
			PopulaceSaveCodec::Save(*populace2, *map2, nullptr, second);
			bool same = first.Find(SAVE_SECTION_POPULACE)->data == second.Find(SAVE_SECTION_POPULACE)->data;
			int households = 0;
			for (const auto& [name, building] : map2->GetBuildings())
				for (Room* room : building->GetRooms()) households += static_cast<int>(room->GetHouseholds().size());
			printf("[SaveCheck] 人口段往返字节一致=%d(%zu字节) 读回后户数=%d\n", same,
				first.Find(SAVE_SECTION_POPULACE)->data.size(), households);
		}
	}

	CheckHouseholdRules(city);

	// 社会段(含账本)往返:存 -> 读回 -> 再存,字节必须一致;读回的账本总和仍为0。
	// 读档会把市民的Job重新指向新建的岗位,所以放在最后做,读回的Society不再释放。
	{
		SaveArchive first;
		SocietySaveCodec::Save(society, city, populace, first);
		string error;
		Society* restored = SocietySaveCodec::Load(first, city, populace, {}, error);
		if (!restored) {
			printf("[SaveCheck] 社会段读档失败: %s\n", error.c_str());
		}
		else {
			SaveArchive second;
			SocietySaveCodec::Save(*restored, city, populace, second);
			bool same = first.Find(SAVE_SECTION_SOCIETY)->data == second.Find(SAVE_SECTION_SOCIETY)->data;
			// 股东:读回后每个组织的股东(市民+占股)必须与原来一致。
			bool shareholdersSame = restored->GetOrganizations().size() == society.GetOrganizations().size();
			for (size_t i = 0; shareholdersSame && i < society.GetOrganizations().size(); i++) {
				const auto& x = society.GetOrganizations()[i]->GetShareholders();
				const auto& y = restored->GetOrganizations()[i]->GetShareholders();
				shareholdersSame = x.size() == y.size();
				for (size_t k = 0; shareholdersSame && k < x.size(); k++)
					shareholdersSame = x[k].citizen == y[k].citizen && x[k].shares == y[k].shares;
			}
			printf("[SaveCheck] 股东往返一致=%d\n", shareholdersSame);
			// 岗位:每个组织的Job数、closed标记、占用者、岗位计划(vacancies)和lastSettledBalance读回后必须一致。
			bool jobsSame = restored->GetOrganizations().size() == society.GetOrganizations().size();
			int closedJobs = 0;
			size_t planEntries = 0;
			for (size_t i = 0; jobsSame && i < society.GetOrganizations().size(); i++) {
				const Organization* x = society.GetOrganizations()[i];
				const Organization* y = restored->GetOrganizations()[i];
				jobsSame = x->GetJobs().size() == y->GetJobs().size() && x->GetLastSettledBalance() == y->GetLastSettledBalance();
				if (x->GetMod() && y->GetMod()) {
					const auto& vx = x->GetMod()->vacancies;
					const auto& vy = y->GetMod()->vacancies;
					jobsSame = jobsSame && vx.size() == vy.size();
					planEntries += vx.size();
					for (size_t k = 0; jobsSame && k < vx.size(); k++) jobsSame = vx[k].room == vy[k].room && vx[k].type == vy[k].type;
				}
				for (size_t k = 0; jobsSame && k < x->GetJobs().size(); k++) {
					const Job* a = x->GetJobs()[k];
					const Job* b = y->GetJobs()[k];
					if (!a->IsOpen()) closedJobs++;
					jobsSame = a->IsOpen() == b->IsOpen() && a->GetType() == b->GetType() && (a->GetOccupant() == nullptr) == (b->GetOccupant() == nullptr);
				}
			}
			printf("[SaveCheck] 岗位往返一致=%d (已关闭岗位%d个, 岗位计划条目%zu条)\n", jobsSame, closedJobs, planEntries);
			const auto& da = first.Find(SAVE_SECTION_SOCIETY)->data;
			const auto& db = second.Find(SAVE_SECTION_SOCIETY)->data;
			size_t diff = 0;
			while (diff < da.size() && diff < db.size() && da[diff] == db[diff]) diff++;
			size_t ledgerBytes = 8 + 8 + 4 + 4 + 12 * (society.GetLedger().GetCitizenBalances().size() + society.GetLedger().GetOrganizationBalances().size());
			// 账本在段尾:首个差异必须落在账本之前(组织mod名字会随命名计数器变,不是账本的问题)。
			bool ledgerSame = da.size() >= ledgerBytes && db.size() >= ledgerBytes &&
				std::equal(da.end() - ledgerBytes, da.end(), db.end() - ledgerBytes);
			printf("[SaveCheck] 社会段账本部分字节一致=%d 首个差异字节=%zu/%zu\n", ledgerSame, diff, da.size());
			printf("[SaveCheck] 社会段往返字节一致=%d 读回账本总和=%s 市民%s 组织%s 城市外%s\n", same,
				FormatMoney(restored->GetLedger().Total()).c_str(),
				FormatMoney(restored->GetLedger().CitizensTotal()).c_str(),
				FormatMoney(restored->GetLedger().OrganizationsTotal()).c_str(),
				FormatMoney(restored->GetLedger().Balance(Party::Outside())).c_str());
		}
	}
	return 0;
}
