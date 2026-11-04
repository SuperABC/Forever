#include "society/economy.h"

#include "society/society.h"
#include "society/organization.h"
#include "society/job.h"
#include "industry/industry.h"
#include "map/component.h"
#include "map/room.h"
#include "populace/citizen.h"
#include "populace/household.h"
#include "story/script.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>


using namespace std;

namespace {
	// 本版市民只需要吃饭：每人每天1份food服务。以后有更多需求类型时扩展这里。
	constexpr const char* kMealServiceType = "food";

	// 一笔钱平均分成n份，余数（分）依次多给前几份，总额不变。
	vector<int64_t> SplitEvenly(int64_t amount, size_t parts) {
		vector<int64_t> shares(parts, parts ? amount / static_cast<int64_t>(parts) : 0);
		for (size_t i = 0; parts && i < static_cast<size_t>(amount % static_cast<int64_t>(parts)); i++) shares[i]++;
		return shares;
	}

	struct Provider {
		Organization* organization = nullptr;
		ServiceOffer offer;
		int quota = 0;
		int sold = 0;
		int64_t revenue = 0;
	};

	// 组织名下的仓库：组织占用的Component里的房间生成的Storage。
	vector<Storage*> StoragesOf(const Organization* organization, const Industry& industry) {
		vector<Storage*> storages;
		for (const Component* component : organization->GetComponents()) {
			if (!component) continue;
			for (const Room* room : component->GetRooms()) {
				if (Storage* storage = industry.GetRoomStorage(room)) storages.push_back(storage);
			}
		}
		return storages;
	}

	int StockOf(const vector<Storage*>& storages, const string& type) {
		float total = 0.f;
		for (const Storage* storage : storages) total += storage->GetAmount(type);
		return total > 0.f ? static_cast<int>(total + 1e-4f) : 0;
	}
}

DailyReport Economy::Daily(Society& society, Industry& industry, const std::vector<Room*>& rooms,
	const std::vector<Citizen*>& citizens, const Time& time, PostHandle* post, int currentYear) {
	DailyReport report;

	// 跨月先把上个月的分红结掉，再开始今天的日结。
	int monthKey = time.GetYear() * 12 + time.GetMonth();
	if (society.GetLastMonthKey() == 0) {
		society.SetLastMonthKey(monthKey);
	}
	else if (monthKey != society.GetLastMonthKey()) {
		report.month = Monthly(society);
		society.SetLastMonthKey(monthKey);
	}

	UpdateBankruptcies(society, citizens, currentYear, report);
	SyncIndustry(society, industry);

	report.goods = Settlement::SettleGoods(society, industry);
	RunServices(society, industry, rooms, post, currentYear, report);
	report.rent = Settlement::PayRents(society, rooms, currentYear);
	PayWages(society, report);
	PaySubsidies(society, citizens, currentYear, report);
	return report;
}

void Economy::RunServices(Society& society, Industry& industry, const vector<Room*>& rooms,
	PostHandle* post, int currentYear, DailyReport& report) {
	Ledger& ledger = society.GetLedger();

	vector<string> stockTypes;
	for (Product* product : industry.GetProducts()) stockTypes.push_back(product->GetType());

	// 一、把各组织仓库里的货物写进它的script：self.stock.<货物类型> = 整数库存。
	unordered_map<Organization*, unordered_map<string, int>> before;
	vector<Organization*> sellers;
	for (Organization* organization : society.GetOrganizations()) {
		if (!organization->GetMod() || !organization->GetScript() || organization->IsBankrupt()) continue;
		sellers.push_back(organization);
		vector<Storage*> storages = StoragesOf(organization, industry);
		for (const string& type : stockTypes) {
			int stock = StockOf(storages, type);
			before[organization][type] = stock;
			organization->GetScript()->SetValue("stock." + type, ValueType(stock));
		}
	}

	// 二、收集全城组织当日能提供的服务，按服务类型归类。
	map<string, vector<Provider>> providers;
	for (Organization* organization : sellers) {
		OrganizationMod* mod = organization->GetMod();
		mod->CollectServices(organization->GetScript(), stockTypes, post);
		for (const ServiceOffer& offer : mod->services) {
			if (offer.capacity <= 0) continue;
			Provider provider;
			provider.organization = organization;
			provider.offer = offer;
			providers[offer.type].push_back(provider);
		}
	}

	// 三、市民按户反馈需求：每人1份food。
	struct Need {
		Household* household;
		int quantity;
	};
	map<string, vector<Need>> needs;
	for (const Room* room : rooms) {
		if (!room || !room->IsResidential()) continue;
		for (Household* household : room->GetHouseholds()) {
			// 成年人每天1份，未成年(<adultAge)吃一半，整户合计向上取整到整份。
			int halfPortions = 0;
			for (const Citizen* member : household->GetMembers()) {
				if (!member || member->IsDead()) continue;
				halfPortions += member->GetAge(currentYear) < society.GetPolicy().adultAge ? 1 : 2;
			}
			int portions = (halfPortions + 1) / 2;
			if (portions > 0) needs[kMealServiceType].push_back({ household, portions });
		}
	}

	// 四、按服务类型把需求加权平均分给提供者：权重就是各自当日能提供的份数，供给不足时每家卖光，
	// 供给充足时按份数比例分。
	for (auto& [type, typeNeeds] : needs) {
		ServiceStat& stat = report.services[type];
		long long demand = 0;
		for (const Need& need : typeNeeds) demand += need.quantity;
		stat.demand = static_cast<int>(demand);

		vector<Provider>& typeProviders = providers[type];
		long long capacity = 0;
		for (const Provider& provider : typeProviders) capacity += provider.offer.capacity;

		if (capacity > 0) {
			if (demand >= capacity) {
				for (Provider& provider : typeProviders) provider.quota = provider.offer.capacity;
			}
			else {
				// 按份数比例分，余数按小数部分从大到小逐份补。
				long long assigned = 0;
				vector<pair<long long, size_t>> remainders;
				for (size_t i = 0; i < typeProviders.size(); i++) {
					long long scaled = demand * typeProviders[i].offer.capacity;
					typeProviders[i].quota = static_cast<int>(scaled / capacity);
					assigned += typeProviders[i].quota;
					remainders.push_back({ scaled % capacity, i });
				}
				stable_sort(remainders.begin(), remainders.end(),
					[](const auto& a, const auto& b) { return a.first > b.first; });
				for (size_t k = 0; assigned < demand && k < remainders.size(); k++, assigned++) {
					typeProviders[remainders[k].second].quota++;
				}
			}
		}

		// 五、各户依次从提供者的配额里买：成年成员均摊付款。市民账户允许为负，每人每天1份food必须满足，只受库存限制。
		for (const Need& need : typeNeeds) {
			vector<Citizen*> payers = need.household->GetAdults(currentYear);
			if (payers.empty()) payers = need.household->GetMembers();
			int remaining = need.quantity;
			for (Provider& provider : typeProviders) {
				if (remaining <= 0) break;
				int64_t price = provider.offer.unitPrice;
				int take = min(remaining, provider.quota);
				if (take <= 0) continue;

				int64_t cost = price * take;
				vector<int64_t> shares = SplitEvenly(cost, payers.size());
				for (size_t k = 0; k < payers.size(); k++) {
					ledger.Transfer(Party::Of(payers[k]), Party::Of(provider.organization), shares[k], Flow::SERVICE_PURCHASE);
				}
				provider.quota -= take;
				provider.sold += take;
				provider.revenue += cost;
				remaining -= take;
			}
			stat.unmet += remaining;
		}
	}

	// 六、组织扣除被用掉的服务（mod只改script变量），再按变量值回写仓库库存。
	for (auto& [type, typeProviders] : providers) {
		ServiceStat& stat = report.services[type];
		for (Provider& provider : typeProviders) {
			if (provider.sold <= 0) continue;
			provider.organization->GetMod()->ApplyServiceUsage(provider.offer.name, provider.sold,
				provider.organization->GetScript());
			// 消费税即时缴纳：商店每笔销售当场把销售额的一部分交给政府。
			int64_t consumptionTax = society.GetPolicy().Tax(provider.revenue, society.GetPolicy().consumptionTaxPercent);
			society.GetLedger().Transfer(Party::Of(provider.organization), Party::Government(), consumptionTax, Flow::CONSUMPTION_TAX);
			stat.sold += provider.sold;
			stat.revenue += provider.revenue;
			stat.consumptionTax += consumptionTax;
		}
	}
	for (Organization* organization : sellers) {
		vector<Storage*> storages = StoragesOf(organization, industry);
		for (const string& type : stockTypes) {
			auto [exist, value] = organization->GetScript()->GetValue("stock." + type);
			if (!exist) continue;
			int removed = before[organization][type] - ToInt(value);
			for (Storage* storage : storages) {
				if (removed <= 0) break;
				removed -= static_cast<int>(storage->Output(type, static_cast<float>(removed)));
			}
		}
	}
}

void Economy::PayWages(Society& society, DailyReport& report) {
	Ledger& ledger = society.GetLedger();
	for (Organization* organization : society.GetOrganizations()) {
		OrganizationMod* mod = organization->GetMod();
		if (!mod || mod->wage <= 0 || organization->IsBankrupt()) continue; // 倒闭企业停发工资，员工不离职
		for (Job* job : organization->GetJobs()) {
			Citizen* worker = job ? job->GetOccupant() : nullptr;
			if (!worker || worker->IsDead()) continue;
			ledger.Transfer(Party::Of(organization), Party::Of(worker), mod->wage, Flow::WAGE);
			// 所得税即时缴纳：发工资时当场代扣。
			int64_t incomeTax = society.GetPolicy().Tax(mod->wage, society.GetPolicy().incomeTaxPercent);
			ledger.Transfer(Party::Of(worker), Party::Government(), incomeTax, Flow::INCOME_TAX);
			report.wageIncomeTax += incomeTax;
			report.wages += mod->wage;
			report.wagesPaid++;
		}
	}
}

void Economy::PaySubsidies(Society& society, const vector<Citizen*>& citizens, int currentYear,
	DailyReport& report) {
	Ledger& ledger = society.GetLedger();
	const GovernmentPolicy& policy = society.GetPolicy();

	for (Citizen* citizen : citizens) {
		if (!citizen || citizen->IsDead()) continue;
		int age = citizen->GetAge(currentYear);
		if (age < policy.adultAge) {
			// 未成年补助发给户里的成年人(均摊)，因为吃饭房租都是成年人付；户里没有成年人时才发给孩子自己。
			int64_t allowance = policy.Allowance(policy.minorAllowance);
			vector<Citizen*> guardians;
			if (citizen->GetHousehold()) guardians = citizen->GetHousehold()->GetAdults(currentYear);
			if (guardians.empty()) {
				ledger.Transfer(Party::Government(), Party::Of(citizen), allowance, Flow::MINOR_ALLOWANCE);
			}
			else {
				vector<int64_t> shares = SplitEvenly(allowance, guardians.size());
				for (size_t k = 0; k < guardians.size(); k++) {
					ledger.Transfer(Party::Government(), Party::Of(guardians[k]), shares[k], Flow::MINOR_ALLOWANCE);
				}
			}
			report.minorAllowance += allowance;
			report.minorCount++;
		}
		else if (age >= policy.retirementAge) {
			int64_t allowance = policy.Allowance(policy.pension);
			ledger.Transfer(Party::Government(), Party::Of(citizen), allowance, Flow::PENSION);
			report.pensions += allowance;
			report.pensionCount++;
		}
		else if (!citizen->GetJob()) {
			int64_t allowance = policy.Allowance(policy.unemploymentBenefit);
			ledger.Transfer(Party::Government(), Party::Of(citizen), allowance, Flow::UNEMPLOYMENT);
			report.unemployment += allowance;
			report.unemploymentCount++;
		}
	}
}

MonthlyReport Economy::Monthly(Society& society) {
	MonthlyReport report;
	report.settled = true;
	Ledger& ledger = society.GetLedger();
	const GovernmentPolicy& policy = society.GetPolicy();

	// 企业账户超过初始资金的部分是可分配利润：先缴企业税，税后按占股比例分给股东，少于初始资金不分红。
	// 股东拿到分红时当场缴所得税（税收一律即时缴纳）。
	for (Organization* organization : society.GetOrganizations()) {
		OrganizationMod* mod = organization->GetMod();
		if (!mod || organization->IsBankrupt()) continue;
		int64_t surplus = ledger.Balance(Party::Of(organization)) - mod->initialFunds;
		const vector<Organization::Shareholder>& shareholders = organization->GetShareholders();
		if (surplus <= 0 || shareholders.empty()) continue;

		int64_t corporateTax = policy.Tax(surplus, policy.corporateTaxPercent);
		ledger.Transfer(Party::Of(organization), Party::Government(), corporateTax, Flow::CORPORATE_TAX);
		report.corporateTax += corporateTax;

		int64_t distributable = surplus - corporateTax;
		int64_t paid = 0;
		for (size_t i = 0; i < shareholders.size(); i++) {
			int64_t dividend = static_cast<int64_t>(static_cast<double>(distributable) * shareholders[i].ratio);
			if (i + 1 == shareholders.size()) dividend = distributable - paid; // 最后一位拿走余数，总额不丢分
			if (dividend <= 0) continue;
			ledger.Transfer(Party::Of(organization), Party::Of(shareholders[i].citizen), dividend, Flow::DIVIDEND);
			int64_t incomeTax = policy.Tax(dividend, policy.incomeTaxPercent);
			ledger.Transfer(Party::Of(shareholders[i].citizen), Party::Government(), incomeTax, Flow::INCOME_TAX);
			report.dividendIncomeTax += incomeTax;
			paid += dividend;
		}
		report.dividends += paid;
	}
	return report;
}

void Economy::SyncIndustry(Society& society, Industry& industry) {
	unordered_set<const Room*> closed;
	for (const Organization* organization : society.GetOrganizations()) {
		if (!organization->IsBankrupt()) continue;
		for (const Component* component : organization->GetComponents()) {
			if (!component) continue;
			for (const Room* room : component->GetRooms()) closed.insert(room);
		}
	}
	industry.SetClosedRooms(closed);

	// 到岗率：房间里实际有人的岗位数/组织指定的岗位数(只统计有岗位的房间)，工坊产能按它走。
	unordered_map<const Room*, int> designed, filled;
	for (const Organization* organization : society.GetOrganizations()) {
		for (const Job* job : organization->GetJobs()) {
			if (!job || !job->GetPosition()) continue;
			designed[job->GetPosition()]++;
			if (job->GetOccupant()) filled[job->GetPosition()]++;
		}
	}
	unordered_map<const Room*, float> staffing;
	for (const auto& [room, count] : designed) staffing[room] = static_cast<float>(filled[room]) / count;
	industry.SetStaffingRatios(staffing);

	// 三重负反馈，都以开局外部注资为基准：
	// 1) 出口阈值 = 1-1/(x+1)，x=当前全部注资/开局注资：城里的钱越多，工坊产出仓留得越多，出口越少；
	// 2) 税率系数 = 1-atan(g)*2/pi，g=政府余额/开局注资：盈余降税(最低0)、负债加税(最高2倍)；
	// 3) 补助系数 = exp(g)：盈余多发、负债少发。
	const Ledger& ledger = society.GetLedger();
	double x = ledger.InjectionRatio();
	industry.SetExportKeepRatio(static_cast<float>(1.0 - 1.0 / (x + 1.0)));
	double g = ledger.GovernmentRatio();
	GovernmentPolicy& policy = society.GetPolicy();
	policy.taxFactor = 1.0 - atan(g) / 3.14159265358979323846 * 2.0;
	policy.allowanceFactor = exp(g);
}

void Economy::UpdateBankruptcies(Society& society, const vector<Citizen*>& citizens, int currentYear,
	DailyReport& report) {
	Ledger& ledger = society.GetLedger();

	// 一、资金为负的企业倒闭。
	for (Organization* organization : society.GetOrganizations()) {
		if (!organization->IsBankrupt() && ledger.Balance(Party::Of(organization)) < 0) {
			organization->SetBankrupt(true);
			report.newBankruptcies++;
		}
	}

	// 二、倒闭的企业：每个股东位都要找到一位存款超过"初始资金×占股比例"的成年市民（同一人只担一个股东位），
	// 全部找到才重启。
	vector<Citizen*> adults;
	for (Citizen* citizen : citizens) {
		if (citizen && !citizen->IsDead() && citizen->GetAge(currentYear) >= society.GetPolicy().adultAge) {
			adults.push_back(citizen);
		}
	}

	for (Organization* organization : society.GetOrganizations()) {
		OrganizationMod* mod = organization->GetMod();
		if (!organization->IsBankrupt() || !mod) continue;
		const vector<Organization::Shareholder>& slots = organization->GetShareholders();
		if (slots.empty()) continue;

		unordered_set<const Citizen*> taken;
		vector<Organization::Shareholder> chosen;
		bool complete = true;
		for (const Organization::Shareholder& slot : slots) {
			int64_t required = static_cast<int64_t>(static_cast<double>(mod->initialFunds) * slot.ratio);
			vector<Citizen*> candidates;
			for (Citizen* citizen : adults) {
				if (!taken.count(citizen) && ledger.Balance(Party::Of(citizen)) > required) candidates.push_back(citizen);
			}
			if (candidates.empty()) {
				complete = false;
				break;
			}
			Citizen* picked = candidates[GetRandom(static_cast<int>(candidates.size()))];
			taken.insert(picked);
			chosen.push_back({ picked, slot.ratio });
		}
		if (!complete) continue;

		// 核销旧负债（城市外账户承担），股东按占股比例集齐初始资金，重新营业。
		int64_t debt = -ledger.Balance(Party::Of(organization));
		if (debt > 0) {
			ledger.Transfer(Party::Outside(), Party::Of(organization), debt, Flow::DEBT_WRITE_OFF);
			report.debtWrittenOff += debt;
		}
		int64_t contributed = 0;
		for (size_t i = 0; i < chosen.size(); i++) {
			int64_t share = static_cast<int64_t>(static_cast<double>(mod->initialFunds) * chosen[i].ratio);
			if (i + 1 == chosen.size()) share = mod->initialFunds - contributed; // 最后一位补齐，总额正好等于初始资金
			ledger.Transfer(Party::Of(chosen[i].citizen), Party::Of(organization), share, Flow::RESTART_FUNDS);
			contributed += share;
		}
		organization->SetShareholders(chosen);
		organization->SetBankrupt(false);
		report.restarts++;
		report.restartFunds += contributed;
	}

	for (const Organization* organization : society.GetOrganizations()) {
		if (organization->IsBankrupt()) report.bankruptTotal++;
	}
}
