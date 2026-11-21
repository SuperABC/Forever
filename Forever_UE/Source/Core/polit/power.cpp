#include "polit/power.h"

#include "polit/policy_ids.h"


using namespace std;

void PowerCatalog::Register(const PowerDef& def) {
	for (PowerDef& existing : defs) {
		if (existing.id == def.id) {
			existing = def;
			return;
		}
	}
	defs.push_back(def);
}

const PowerDef* PowerCatalog::Find(const string& id) const {
	for (const PowerDef& def : defs) {
		if (def.id == id) return &def;
	}
	return nullptr;
}

void PowerCatalog::RegisterDefaults() {
	Register({ "manage_corporate_tax", "征收企业税", PolicyId::kCorporateTax });
	Register({ "manage_income_tax", "征收所得税", PolicyId::kIncomeTax });
	Register({ "manage_consumption_tax", "征收消费税", PolicyId::kConsumptionTax });
	Register({ "manage_rent_tax", "征收房租税", PolicyId::kRentTax });
	Register({ "manage_import_tariff", "征收进口关税", PolicyId::kImportTariff });
	Register({ "manage_retirement_pension", "发放养老金", PolicyId::kRetirementPension });
	Register({ "manage_unemployment_benefit", "发放失业补助", PolicyId::kUnemploymentBenefit });
	Register({ "manage_minor_allowance", "发放未成年补助", PolicyId::kMinorAllowance });
	Register({ "manage_immigration_ratio", "批准移民", PolicyId::kImmigrationRatio });
}
