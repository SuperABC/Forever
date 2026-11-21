#include "polit/policy.h"

#include "polit/policy_ids.h"

#include <algorithm>
#include <cmath>


using namespace std;

int64_t TaxOf(int64_t amount, double percent) {
	if (amount <= 0 || percent <= 0.0) return 0;
	return static_cast<int64_t>(std::floor(static_cast<double>(amount) * percent / 100.0 + 1e-9));
}

int64_t GovernmentPolicy::Tax(int64_t amount, double basePercent) const {
	return TaxOf(amount, basePercent * taxFactor);
}

int64_t GovernmentPolicy::Allowance(int64_t base) const {
	return static_cast<int64_t>(std::llround(static_cast<double>(base) * allowanceFactor));
}

int64_t GovernmentPolicy::Tariff(int64_t cost) const {
	if (cost <= 0 || tariffPercent <= 0.0) return 0;
	return static_cast<int64_t>(std::floor(static_cast<double>(cost) * tariffPercent / (100.0 + tariffPercent) + 1e-9));
}

namespace {
	constexpr double kMinRatio = 0.25; // 政策值下限 = 默认值 × 此值
	constexpr double kMaxRatio = 4.0;  // 上限 = 默认值 × 此值
	constexpr double kMaxPercent = 100.0;

	PolicyDef Make(const char* id, const char* name, PolicyDef::Kind kind, double (*get)(const GovernmentPolicy&),
		void (*set)(GovernmentPolicy&, double)) {
		PolicyDef def;
		def.id = id;
		def.name = name;
		def.kind = kind;
		def.get = get;
		def.set = set;
		const GovernmentPolicy defaults;
		def.defaultValue = get(defaults);
		def.minValue = def.defaultValue * kMinRatio;
		def.maxValue = def.defaultValue * kMaxRatio;
		if (kind == PolicyDef::TAX) def.maxValue = min(def.maxValue, kMaxPercent);
		return def;
	}
}

const vector<PolicyDef>& PolicyCatalog::All() {
	static const vector<PolicyDef> defs = {
		Make(PolicyId::kCorporateTax, "企业税", PolicyDef::TAX,
			[](const GovernmentPolicy& p) { return p.corporateTaxPercent; },
			[](GovernmentPolicy& p, double v) { p.corporateTaxPercent = v; }),
		Make(PolicyId::kIncomeTax, "所得税", PolicyDef::TAX,
			[](const GovernmentPolicy& p) { return p.incomeTaxPercent; },
			[](GovernmentPolicy& p, double v) { p.incomeTaxPercent = v; }),
		Make(PolicyId::kConsumptionTax, "消费税", PolicyDef::TAX,
			[](const GovernmentPolicy& p) { return p.consumptionTaxPercent; },
			[](GovernmentPolicy& p, double v) { p.consumptionTaxPercent = v; }),
		Make(PolicyId::kRentTax, "房租税", PolicyDef::TAX,
			[](const GovernmentPolicy& p) { return p.rentTaxPercent; },
			[](GovernmentPolicy& p, double v) { p.rentTaxPercent = v; }),
		Make(PolicyId::kImportTariff, "进口关税", PolicyDef::TAX,
			[](const GovernmentPolicy& p) { return p.tariffPercent; },
			[](GovernmentPolicy& p, double v) { p.tariffPercent = v; }),
		Make(PolicyId::kRetirementPension, "养老金", PolicyDef::BENEFIT,
			[](const GovernmentPolicy& p) { return static_cast<double>(p.pension); },
			[](GovernmentPolicy& p, double v) { p.pension = static_cast<int64_t>(std::llround(v)); }),
		Make(PolicyId::kUnemploymentBenefit, "失业补助", PolicyDef::BENEFIT,
			[](const GovernmentPolicy& p) { return static_cast<double>(p.unemploymentBenefit); },
			[](GovernmentPolicy& p, double v) { p.unemploymentBenefit = static_cast<int64_t>(std::llround(v)); }),
		Make(PolicyId::kMinorAllowance, "未成年补助", PolicyDef::BENEFIT,
			[](const GovernmentPolicy& p) { return static_cast<double>(p.minorAllowance); },
			[](GovernmentPolicy& p, double v) { p.minorAllowance = static_cast<int64_t>(std::llround(v)); }),
		Make(PolicyId::kImmigrationRatio, "移民目标比例", PolicyDef::IMMIGRATION,
			[](const GovernmentPolicy& p) { return p.immigrationTargetRatio; },
			[](GovernmentPolicy& p, double v) { p.immigrationTargetRatio = v; }),
	};
	return defs;
}

const PolicyDef* PolicyCatalog::Find(const string& id) {
	for (const PolicyDef& def : All()) {
		if (def.id == id) return &def;
	}
	return nullptr;
}

double PolicyCatalog::Apply(GovernmentPolicy& policy, const PolicyDef& def, double value) {
	value = max(def.minValue, min(def.maxValue, value));
	def.set(policy, value);
	return def.get(policy);
}
