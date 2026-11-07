#pragma once

#include "class.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>


// 一个账户的持有者：城市外、政府、某个市民、某个组织。
struct Party {
	enum Kind { OUTSIDE, GOVERNMENT, CITIZEN, ORGANIZATION };

	Kind kind = OUTSIDE;
	const void* ref = nullptr; // CITIZEN/ORGANIZATION时是对应对象的指针

	static Party Outside() { return { OUTSIDE, nullptr }; }
	static Party Government() { return { GOVERNMENT, nullptr }; }
	static Party Of(const Citizen* citizen) { return { CITIZEN, citizen }; }
	static Party Of(const Organization* organization) { return { ORGANIZATION, organization }; }
};

// 每笔转账的类别，用于统计各类账户的收入/支出明细。
enum class Flow {
	INJECTION,        // 城市外注资（开局）
	GOODS_INTERNAL,   // 城内货款（批发）
	GOODS_IMPORT,     // 进口货款（付给城市外）
	GOODS_EXPORT,     // 出口货款（城市外付出）
	TARIFF,           // 关税
	SERVICE_PURCHASE, // 市民买服务（吃饭）
	CONSUMPTION_TAX,  // 消费税
	RENT,             // 房租
	RENT_TAX,         // 房租税
	WAGE,             // 工资
	INCOME_TAX,       // 所得税（工资、分红）
	PENSION,          // 养老金
	UNEMPLOYMENT,     // 失业补助
	MINOR_ALLOWANCE,  // 未成年补助
	CORPORATE_TAX,    // 企业税
	DIVIDEND,         // 分红
	RESTART_FUNDS,    // 重启集资
	DEBT_WRITE_OFF,   // 重启时核销的旧负债
	COUNT
};
const char* FlowName(Flow flow); // 中文名

// 全城账本：所有金额int64、单位分。资金只能经Transfer在两个账户之间转移，所以整个账本的
// 总和永远是0（城市外账户为负，等于对城内的净注资），Total()用来每日校验。详见ledger.md。
class Ledger {
	friend class SocietySaveCodec;
public:
	int64_t Balance(const Party& party) const;

	// 开局外部给全城的注资总额(分，正数)，InitFunds注资完记下，是三重负反馈的基准。状态，随社会段存档。
	int64_t GetInitialInjection() const { return initialInjection; }
	void SetInitialInjection(int64_t value) { initialInjection = value; }
	// 当前全部注资(城市外账户余额取反)与开局注资之比x，范围[0,+inf)；没记过开局注资时返回1。
	double InjectionRatio() const;
	// 政府余额与开局注资之比，符号和政府余额相同；没记过开局注资时返回0。
	double GovernmentRatio() const;

	// from付给to amount分。amount<=0或者from==to不做任何事。不检查余额：账户可以变成负数
	// （企业资金为负即倒闭，由上层判断）。
	void Transfer(const Party& from, const Party& to, int64_t amount, Flow kind);

	// 累计流水：某类别、从某类账户到某类账户的累计金额（分），报表用月初/月末两次快照相减得到当月值。
	int64_t FlowTotal(Flow kind, Party::Kind from, Party::Kind to) const;
	const std::array<int64_t, static_cast<size_t>(Flow::COUNT) * 16>& GetFlowTotals() const { return flowTotals; }

	// 全部账户之和，正常情况下恒为0。
	int64_t Total() const;

	// 分类汇总，供报表使用。
	int64_t CitizensTotal() const;
	int64_t OrganizationsTotal() const;

	const std::unordered_map<const Citizen*, int64_t>& GetCitizenBalances() const { return citizens; }
	const std::unordered_map<const Organization*, int64_t>& GetOrganizationBalances() const { return organizations; }

private:
	int64_t& Slot(const Party& party);

	std::array<int64_t, static_cast<size_t>(Flow::COUNT) * 16> flowTotals{}; // [类别][付款方类][收款方类]
	int64_t outside = 0;
	int64_t initialInjection = 0;
	int64_t government = 0;
	std::unordered_map<const Citizen*, int64_t> citizens;
	std::unordered_map<const Organization*, int64_t> organizations;
};

// 分转成"元"的显示字符串，如 1234567 -> "12345.67元"。内部一律用分，只有显示时才除以100。
std::string FormatMoney(int64_t cents);
