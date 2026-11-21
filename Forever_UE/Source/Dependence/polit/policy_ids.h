#pragma once


// 政策id：Core的PolicyCatalog、党派mod的提议(PartyMod::Propose)和权力词汇(PowerInfo::policy)共用，
// 统一"两个单词_小写下划线"。详见Source/Core/polit/policy.md。
namespace PolicyId {
	constexpr const char* kCorporateTax = "corporate_tax";               // 企业税率(%)
	constexpr const char* kIncomeTax = "income_tax";                     // 所得税率(%)
	constexpr const char* kConsumptionTax = "consumption_tax";           // 消费税率(%)
	constexpr const char* kRentTax = "rent_tax";                         // 房租税率(%)
	constexpr const char* kImportTariff = "import_tariff";               // 进口关税率(%)
	constexpr const char* kRetirementPension = "retirement_pension";     // 养老金(分/天)
	constexpr const char* kUnemploymentBenefit = "unemployment_benefit"; // 失业补助(分/天)
	constexpr const char* kMinorAllowance = "minor_allowance";           // 未成年补助(分/天)
	constexpr const char* kImmigrationRatio = "immigration_ratio";       // 移民目标人口比例(目标人口=开局人口×该值)
}
