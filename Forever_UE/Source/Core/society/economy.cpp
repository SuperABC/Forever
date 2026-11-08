#include "society/economy.h"

#include "society/society.h"
#include "society/organization.h"
#include "society/job.h"
#include "society/emigration.h"
#include "industry/industry.h"
#include "map/component.h"
#include "map/room.h"
#include "populace/citizen.h"
#include "populace/household.h"
#include "story/script.h"
#include "player/player.h"
#include "player/asset.h"
#include "industry/product.h"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>


using namespace std;

namespace {
	// 本版市民只需要吃饭：每人每天1份food服务。以后有更多需求类型时扩展这里。
	constexpr const char* kMealServiceType = "food";

	// 增发：总股本最多涨到初始发行量的4倍（是否增发、增发多少由mod在MonthlySettle里决定）。
	constexpr int64_t kMaxShareMultiple = 4;

	// 月结时让mod看一眼现状并就地调整岗位计划/分红率/增发股数，再按计划对账岗位。
	void SettleOrganization(Society& society, Organization* organization, int64_t balance, int64_t profit,
		MonthlyReport& report) {
		OrganizationMod* mod = organization->GetMod();
		vector<Room*> rooms = organization->WorkspaceRooms();
		vector<string> names;
		names.reserve(rooms.size());
		for (const Room* room : rooms) names.push_back(room->GetName());
		vector<RoomState> roomStates(rooms.size());
		for (size_t i = 0; i < rooms.size(); i++) {
			roomStates[i].room = names[i].c_str();
			roomStates[i].capacity = rooms[i]->WorkspaceCapacity();
		}

		OrganizationMonthlyState state;
		state.balance = balance;
		state.profit = profit;
		state.initialFunds = mod->initialFunds;
		state.totalShares = organization->GetTotalShares();
		state.treasuryShares = organization->TreasuryShares();
		state.sharePrice = society.GetMarket().Price(society.GetLedger(), organization);
		state.employees = organization->EmployeeCount();
		state.positions = organization->OpenPositionCount();
		state.rooms = roomStates.data();
		state.roomCount = static_cast<int>(roomStates.size());

		mod->issueShares = 0;
		mod->MonthlySettle(state);
		// 没有岗位计划的mod（外部Mod、不设计岗位的组织）不动岗位。
		if (!mod->vacancies.empty()) {
			Organization::JobChange change = organization->ReconcileJobs();
			report.positionsOpened += change.opened;
			report.positionsClosed += change.closed;
			report.dismissed += change.dismissed;
		}
	}

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
	const std::vector<Citizen*>& citizens, const Time& time, PostHandle* post, int currentYear,
	const Citizen* protectedCitizen) {
	DailyReport report;

	// 跨月先把上个月的分红结掉，再开始今天的日结。
	int monthKey = time.GetYear() * 12 + time.GetMonth();
	if (society.GetLastMonthKey() == 0) {
		society.SetLastMonthKey(monthKey);
	}
	else if (monthKey != society.GetLastMonthKey()) {
		report.month = Monthly(society, rooms, currentYear, protectedCitizen, report);
		society.SetLastMonthKey(monthKey);
	}

	UpdateBankruptcies(society, citizens, currentYear, report);
	report.hired = society.MatchUnemployed(citizens, currentYear);
	SyncIndustry(society, industry);

	report.goods = Settlement::SettleGoods(society, industry);
	RunServices(society, industry, rooms, post, currentYear, report);
	report.rent = Settlement::PayRents(society, rooms, currentYear);
	PayWages(society, report);
	PaySubsidies(society, citizens, currentYear, report);

	// 金融：国债到期还本，市民存取款/挂股票单，撮合成交，最后政府账户为负时发债/印钞补缺口。
	Treasury& treasury = society.GetTreasury();
	treasury.AdvanceDay();
	report.bondRepaid = treasury.RepayMatured(society.GetLedger());
	report.invest = Investor::Daily(society, citizens, currentYear, treasury.GetDay());
	report.stock = society.GetMarket().Clear(society.GetLedger(), society.GetOrganizations());
	FundGovernment(society, citizens, currentYear, report);
	return report;
}

void Economy::FundGovernment(Society& society, const vector<Citizen*>& citizens, int currentYear,
	DailyReport& report) {
	Ledger& ledger = society.GetLedger();
	const GovernmentPolicy& policy = society.GetPolicy();
	int64_t shortfall = -ledger.Balance(Party::Government());
	if (shortfall <= 0) return;

	// 先向市民发国债，利率比存款高。募集的钱进政府账户，但同时记国债负债，不会抬高净头寸。
	report.bondIssued = Investor::SubscribeBonds(society, citizens, currentYear, shortfall,
		policy.depositRateBp + policy.bondPremiumBp, policy.bondTermDays);
	shortfall -= report.bondIssued;

	// 国债补不满的缺口直接印钞：城市外账户直接打给政府，没有负债，净头寸随之回升。
	if (shortfall > 0) {
		ledger.Transfer(Party::Outside(), Party::Government(), shortfall, Flow::PRINT_MONEY);
		report.moneyPrinted = shortfall;
	}
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
				if (!member || !member->IsActive()) continue;
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
			if (payers.empty()) {
				for (Citizen* member : need.household->GetMembers()) {
					if (member && member->IsActive()) payers.push_back(member);
				}
			}
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
		if (!mod || mod->wage <= 0 || organization->IsBankrupt()) continue; // 倒闭企业停发工资（员工在倒闭时已全部裁掉）
		for (Job* job : organization->GetJobs()) {
			Citizen* worker = job ? job->GetOccupant() : nullptr;
			if (!worker || !worker->IsActive()) continue;
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

void Economy::PayWage(Society& society, Organization* organization, Citizen* worker, int64_t amountCents) {
	if (!organization || !worker || amountCents <= 0) return;
	Ledger& ledger = society.GetLedger();
	ledger.Transfer(Party::Of(organization), Party::Of(worker), amountCents, Flow::WAGE);
	int64_t incomeTax = society.GetPolicy().Tax(amountCents, society.GetPolicy().incomeTaxPercent);
	ledger.Transfer(Party::Of(worker), Party::Government(), incomeTax, Flow::INCOME_TAX);
}

vector<Economy::ShopItem> Economy::ListShopItems(Industry& industry, Player& player, const Organization* organization) {
	vector<ShopItem> items;
	if (!organization) return items;
	vector<Storage*> storages = StoragesOf(organization, industry);

	set<string> types;
	for (const Storage* storage : storages) {
		for (const auto& [type, amount] : storage->GetStock()) types.insert(type);
	}
	for (const string& type : types) {
		int stock = StockOf(storages, type);
		if (stock < 1) continue;
		Product* product = industry.FindProduct(type);
		if (!product) continue;
		string assetType = player.FindAssetTypeByProduct(type);
		if (assetType.empty()) continue;
		Asset* sample = player.CreateAsset(assetType);
		if (!sample) continue;

		ShopItem item;
		item.productType = type;
		item.assetType = assetType;
		item.price = product->GetRetailPrice();
		item.stock = stock;
		item.size = sample->GetSize();
		player.DestroyAsset(sample);
		items.push_back(item);
	}
	return items;
}

Economy::PurchaseResult Economy::PlayerPurchaseCart(Society& society, Industry& industry, Player& player,
	Organization* seller, Citizen* buyer, const vector<pair<string, int>>& cart) {
	if (!seller || !buyer) return PurchaseResult::Invalid;

	vector<Storage*> storages = StoragesOf(seller, industry);
	int64_t total = 0;
	vector<string> unitTypes;  // 每一件对应的productType
	vector<string> unitAssets; // 每一件对应的Asset类型
	vector<float> unitSizes;
	for (const auto& [productType, quantity] : cart) {
		if (quantity <= 0) continue;
		Product* product = industry.FindProduct(productType);
		string assetType = player.FindAssetTypeByProduct(productType);
		if (!product || assetType.empty() || quantity > StockOf(storages, productType)) return PurchaseResult::Invalid;
		Asset* sample = player.CreateAsset(assetType);
		if (!sample) return PurchaseResult::Invalid;
		float size = sample->GetSize();
		player.DestroyAsset(sample);

		total += product->GetRetailPrice() * quantity;
		for (int i = 0; i < quantity; i++) {
			unitTypes.push_back(productType);
			unitAssets.push_back(assetType);
			unitSizes.push_back(size);
		}
	}
	if (unitTypes.empty()) return PurchaseResult::Invalid;

	Ledger& ledger = society.GetLedger();
	if (ledger.Balance(Party::Of(buyer)) < total) return PurchaseResult::NoMoney;
	Player::CarryPlan plan = player.PlanCarry(unitSizes);
	if (!plan.feasible) return PurchaseResult::NoSpace;

	// 按方案把每一件放到容器/手上，只对放下的件收钱、扣库存。
	map<string, int> bought;
	int64_t charged = 0;
	for (size_t i = 0; i < unitTypes.size(); i++) {
		Asset* asset = player.CreateAsset(unitAssets[i]);
		if (!asset) continue;
		const Player::CarryTarget& target = plan.targets[i];
		bool placed = false;
		if (target.kind == Player::CarryTarget::Container) placed = target.container->AddContent(asset);
		else if (target.kind == Player::CarryTarget::RightHand) placed = player.AddByPath("right", asset);
		else if (target.kind == Player::CarryTarget::LeftHand) placed = player.AddByPath("left", asset);
		if (!placed) {
			player.DestroyAsset(asset);
			continue;
		}
		bought[unitTypes[i]]++;
		charged += industry.FindProduct(unitTypes[i])->GetRetailPrice();
	}

	for (const auto& [productType, quantity] : bought) {
		float remaining = static_cast<float>(quantity);
		for (Storage* storage : storages) {
			if (remaining <= 0.f) break;
			remaining -= storage->Output(productType, remaining);
		}
	}
	ledger.Transfer(Party::Of(buyer), Party::Of(seller), charged, Flow::SERVICE_PURCHASE);
	int64_t consumptionTax = society.GetPolicy().Tax(charged, society.GetPolicy().consumptionTaxPercent);
	ledger.Transfer(Party::Of(seller), Party::Government(), consumptionTax, Flow::CONSUMPTION_TAX);
	return charged > 0 ? PurchaseResult::Success : PurchaseResult::NoSpace;
}

void Economy::PaySubsidies(Society& society, const vector<Citizen*>& citizens, int currentYear,
	DailyReport& report) {
	Ledger& ledger = society.GetLedger();
	const GovernmentPolicy& policy = society.GetPolicy();

	for (Citizen* citizen : citizens) {
		if (!citizen || !citizen->IsActive() || !citizen->IsInEconomy()) continue;
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

MonthlyReport Economy::Monthly(Society& society, const vector<Room*>& rooms, int currentYear,
	const Citizen* protectedCitizen, DailyReport& dailyReport) {
	MonthlyReport report;
	report.settled = true;
	Ledger& ledger = society.GetLedger();
	const GovernmentPolicy& policy = society.GetPolicy();

	unordered_set<const Organization*> paidThisMonth;
	// 先让mod看当月利润并调整岗位计划/分红率/增发股数（MonthlySettle），再对账岗位。
	// 可分配利润 = min(当月利润, 账户超过初始资金的部分)：先缴企业税，税后按mod的分红率取一部分、按持股数分给股东
	// （库存股的那份留在企业），留存的利润不会下个月再被当成利润重复征税/分红。少于初始资金不分红。
	// 股东拿到分红时当场缴所得税（税收一律即时缴纳）。
	for (Organization* organization : society.GetOrganizations()) {
		OrganizationMod* mod = organization->GetMod();
		if (!mod || organization->IsBankrupt()) continue;
		int64_t balance = ledger.Balance(Party::Of(organization));
		int64_t last = organization->GetLastSettledBalance();
		int64_t profit = balance - (last == Organization::kUnsettled ? mod->initialFunds : last);
		SettleOrganization(society, organization, balance, profit, report);

		int64_t base = min(profit, balance - mod->initialFunds);
		const vector<Organization::Shareholder>& shareholders = organization->GetShareholders();
		if (base <= 0 || shareholders.empty()) continue;
		paidThisMonth.insert(organization);

		int64_t corporateTax = policy.Tax(base, policy.corporateTaxPercent);
		ledger.Transfer(Party::Of(organization), Party::Government(), corporateTax, Flow::CORPORATE_TAX);
		report.corporateTax += corporateTax;

		// 税后利润只发分红率那一部分，其余留在企业账户（抬高净资产，股价随之上涨）。
		float payoutRatio = max(0.f, min(1.f, mod->dividendPayoutRatio));
		int64_t distributable = static_cast<int64_t>(static_cast<double>(base - corporateTax) * payoutRatio);
		int64_t paid = 0;
		for (const Organization::Shareholder& shareholder : shareholders) {
			int64_t dividend = distributable * shareholder.shares / organization->GetTotalShares();
			if (dividend <= 0) continue;
			ledger.Transfer(Party::Of(organization), Party::Of(shareholder.citizen), dividend, Flow::DIVIDEND);
			int64_t incomeTax = policy.Tax(dividend, policy.incomeTaxPercent);
			ledger.Transfer(Party::Of(shareholder.citizen), Party::Government(), incomeTax, Flow::INCOME_TAX);
			report.dividendIncomeTax += incomeTax;
			paid += dividend;
		}
		report.dividends += paid;
		society.GetMarket().RecordDividend(organization, static_cast<double>(distributable) / organization->GetTotalShares());
	}
	// 本月没有分红的组织（亏损、低于初始资金、倒闭）记0，免得沿用过期的股息率。
	for (Organization* organization : society.GetOrganizations()) {
		if (!paidThisMonth.count(organization)) society.GetMarket().RecordDividend(organization, 0.0);
	}

	// 增发：mod在MonthlySettle里给出本月增发的股数，按当前股价向老股东配售，募到的钱进企业账户。
	IssueShares(society, report);

	// 记下月结完成后的余额，下个月的当月利润从它算起。
	for (Organization* organization : society.GetOrganizations()) {
		if (!organization->IsBankrupt()) organization->SetLastSettledBalance(ledger.Balance(Party::Of(organization)));
	}

	// 存款和国债结息（政府付息，市民当场缴所得税）。
	report.interest = society.GetTreasury().PayInterest(ledger, policy);

	// 离城检查：一户里所有在场的成年人净资产都为负，整户搬走。
	vector<Citizen*> departed = Emigration::Monthly(society, rooms, currentYear, protectedCitizen);
	report.departedCitizens = static_cast<int>(departed.size());
	unordered_set<const Household*> households;
	for (Citizen* citizen : departed) households.insert(citizen->GetHousehold());
	report.departedHouseholds = static_cast<int>(households.size());
	dailyReport.departed.insert(dailyReport.departed.end(), departed.begin(), departed.end());
	return report;
}

void Economy::IssueShares(Society& society, MonthlyReport& report) {
	Ledger& ledger = society.GetLedger();
	const GovernmentPolicy& policy = society.GetPolicy();
	for (Organization* organization : society.GetOrganizations()) {
		OrganizationMod* mod = organization->GetMod();
		if (!mod || organization->IsBankrupt() || mod->issueShares <= 0) continue;
		// mod在MonthlySettle里要求的增发股数，总股本最多涨到初始的kMaxShareMultiple倍。
		int64_t total = organization->GetTotalShares();
		int64_t price = society.GetMarket().Price(ledger, organization);
		if (price <= 0) continue;
		int64_t issue = min(mod->issueShares, Organization::kTotalShares * kMaxShareMultiple - total);
		mod->issueShares = 0;
		if (issue <= 0) continue;

		// 新股先全部进库存股；老股东按持股比例优先认购，认购额受现金（扣掉备用金）限制，认不完的留在库存股里等市场买。
		organization->SetTotalShares(total + issue);
		int64_t sold = 0;
		vector<Organization::Shareholder> holders = organization->GetShareholders();
		for (const Organization::Shareholder& holder : holders) {
			int64_t entitlement = issue * holder.shares / total;
			int64_t cash = max<int64_t>(0, ledger.Balance(Party::Of(holder.citizen)) - policy.reserve);
			int64_t take = min(entitlement, cash / price);
			if (take <= 0) continue;
			ledger.Transfer(Party::Of(holder.citizen), Party::Of(organization), take * price, Flow::STOCK_TRADE);
			organization->AdjustShares(holder.citizen, take);
			sold += take;
		}
		report.sharesIssued += issue;
		report.sharesSubscribed += sold;
		report.issueProceeds += sold * price;
	}
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

	// 到岗率：房间里开放岗位上的在岗人数/房间容量(容量=满编，只统计有开放岗位的房间)，工坊产能按它走。
	// 扩招超编时大于1，产能随之升高；容量为0的房间退回按开放岗位数算。
	unordered_map<const Room*, int> open, filled;
	for (const Organization* organization : society.GetOrganizations()) {
		for (const Job* job : organization->GetJobs()) {
			if (!job || !job->IsOpen() || !job->GetPosition()) continue;
			open[job->GetPosition()]++;
			if (job->GetOccupant()) filled[job->GetPosition()]++;
		}
	}
	unordered_map<const Room*, float> staffing;
	for (const auto& [room, count] : open) {
		int denominator = room->WorkspaceCapacity() > 0 ? room->WorkspaceCapacity() : count;
		staffing[room] = static_cast<float>(filled[room]) / denominator;
	}
	industry.SetStaffingRatios(staffing);

	// 三重负反馈，都以开局外部注资为基准：
	// 1) 出口阈值 = 1-1/(x+1)，x=当前全部注资/开局注资：城里的钱越多，工坊产出仓留得越多，出口越少；
	// 2) 税率系数 = 1-atan(g)*2/pi，g=政府净头寸/开局注资（净头寸=政府账户−存款−国债本金，借款不抬高它，印钞会）：盈余降税(最低0)、负债加税(最高2倍)；
	// 3) 补助系数 = exp(g)：盈余多发、负债少发。
	const Ledger& ledger = society.GetLedger();
	double x = ledger.InjectionRatio();
	industry.SetExportKeepRatio(static_cast<float>(1.0 - 1.0 / (x + 1.0)));
	double g = society.GovernmentNetRatio();
	GovernmentPolicy& policy = society.GetPolicy();
	policy.taxFactor = 1.0 - atan(g) / 3.14159265358979323846 * 2.0;
	policy.allowanceFactor = exp(g);
}

void Economy::UpdateBankruptcies(Society& society, const vector<Citizen*>& citizens, int currentYear,
	DailyReport& report) {
	Ledger& ledger = society.GetLedger();
	Treasury& treasury = society.GetTreasury();

	// 一、资金为负的企业倒闭。
	for (Organization* organization : society.GetOrganizations()) {
		if (!organization->IsBankrupt() && ledger.Balance(Party::Of(organization)) < 0) {
			organization->SetBankrupt(true);
			report.dismissedByBankruptcy += organization->LayOffAll(); // 员工全部裁掉，重启后重新招聘
			report.newBankruptcies++;
		}
	}

	// 二、倒闭的企业：每个股东位都要找到一位活期余额加存款超过"初始资金×占股比例"的成年市民（同一人只担一个股东位），
	// 全部找到才重启。出资时活期不够的部分先从存款取。
	vector<Citizen*> adults;
	for (Citizen* citizen : citizens) {
		if (citizen && citizen->IsActive() && citizen->IsInEconomy() && citizen->GetAge(currentYear) >= society.GetPolicy().adultAge) {
			adults.push_back(citizen);
		}
	}

	for (Organization* organization : society.GetOrganizations()) {
		OrganizationMod* mod = organization->GetMod();
		if (!organization->IsBankrupt() || !mod) continue;
		if (mod->shareholders.empty()) continue;

		// 股东位按mod声明的占股比例（归一化），旧股东的持股作废。
		float weightSum = 0.f;
		for (float weight : mod->shareholders) weightSum += weight;
		if (weightSum <= 0.f) continue;

		unordered_set<const Citizen*> taken;
		vector<Citizen*> pickedCitizens;
		vector<float> pickedWeights;
		bool complete = true;
		for (float weight : mod->shareholders) {
			int64_t required = static_cast<int64_t>(static_cast<double>(mod->initialFunds) * weight / weightSum);
			vector<Citizen*> candidates;
			for (Citizen* citizen : adults) {
				if (!taken.count(citizen) && ledger.Balance(Party::Of(citizen)) + treasury.DepositOf(citizen) > required) candidates.push_back(citizen);
			}
			if (candidates.empty()) {
				complete = false;
				break;
			}
			Citizen* picked = candidates[GetRandom(static_cast<int>(candidates.size()))];
			taken.insert(picked);
			pickedCitizens.push_back(picked);
			pickedWeights.push_back(weight);
		}
		if (!complete) continue;
		vector<Organization::Shareholder> chosen = Organization::SplitShares(pickedCitizens, pickedWeights);

		// 核销旧负债（城市外账户承担），股东按占股比例集齐初始资金，重新营业。
		int64_t debt = -ledger.Balance(Party::Of(organization));
		if (debt > 0) {
			ledger.Transfer(Party::Outside(), Party::Of(organization), debt, Flow::DEBT_WRITE_OFF);
			report.debtWrittenOff += debt;
		}
		int64_t contributed = 0;
		for (size_t i = 0; i < chosen.size(); i++) {
			int64_t share = mod->initialFunds * chosen[i].shares / Organization::kTotalShares;
			if (i + 1 == chosen.size()) share = mod->initialFunds - contributed; // 最后一位补齐，总额正好等于初始资金
			int64_t cash = ledger.Balance(Party::Of(chosen[i].citizen));
			if (cash < share) treasury.Withdraw(ledger, chosen[i].citizen, share - cash);
			ledger.Transfer(Party::Of(chosen[i].citizen), Party::Of(organization), share, Flow::RESTART_FUNDS);
			contributed += share;
		}
		organization->SetShareholders(chosen);
		organization->SetTotalShares(Organization::kTotalShares); // 重启后重新发行，增发的股本作废
		society.GetMarket().ResetQuote(organization);
		organization->SetBankrupt(false);
		report.positionsReopened += organization->ReopenInitial().opened; // 岗位恢复到开局满编，由每天的再就业补人
		organization->SetLastSettledBalance(ledger.Balance(Party::Of(organization)));
		report.restarts++;
		report.restartFunds += contributed;
	}

	for (const Organization* organization : society.GetOrganizations()) {
		if (organization->IsBankrupt()) report.bankruptTotal++;
	}
}
