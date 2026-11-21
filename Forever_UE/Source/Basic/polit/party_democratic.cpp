#include "party_democratic.h"

#include "party_common.h"
#include "polit/policy_ids.h"

using namespace std;

// ---- 占位初值(等用户调)：目标值，Core会换算成相对当前值的比例并夹到[0.9, 1.1] ----
// 税率单位%，补助单位分/天，移民目标比例=目标人口/开局人口。
#define DEMOCRATIC_MEMBERS 12 // 党员人数
#define DEMOCRATIC_CORPORATE_TAX 15
#define DEMOCRATIC_INCOME_TAX 12
#define DEMOCRATIC_CONSUMPTION_TAX 5
#define DEMOCRATIC_RENT_TAX 10
#define DEMOCRATIC_IMPORT_TARIFF 15
#define DEMOCRATIC_PENSION 3000
#define DEMOCRATIC_UNEMPLOYMENT 3000
#define DEMOCRATIC_MINOR_ALLOWANCE 1000
#define DEMOCRATIC_IMMIGRATION 2.0

int DemocraticParty::count = 0;

DemocraticParty::DemocraticParty() : id(count++) {
}

const char* DemocraticParty::GetName() {
	name = "DemocraticParty" + to_string(id);
	return name.data();
}

void DemocraticParty::SetProperty() {
	memberCount = DEMOCRATIC_MEMBERS;
}

void DemocraticParty::Propose(const vector<PolicyValue>& current) {
	(void)current; // 固定目标值：不看当前值，永远提议同一组数
	targets.clear();
	targets.push_back({ PolicyId::kCorporateTax, DEMOCRATIC_CORPORATE_TAX });
	targets.push_back({ PolicyId::kIncomeTax, DEMOCRATIC_INCOME_TAX });
	targets.push_back({ PolicyId::kConsumptionTax, DEMOCRATIC_CONSUMPTION_TAX });
	targets.push_back({ PolicyId::kRentTax, DEMOCRATIC_RENT_TAX });
	targets.push_back({ PolicyId::kImportTariff, DEMOCRATIC_IMPORT_TARIFF });
	targets.push_back({ PolicyId::kRetirementPension, DEMOCRATIC_PENSION });
	targets.push_back({ PolicyId::kUnemploymentBenefit, DEMOCRATIC_UNEMPLOYMENT });
	targets.push_back({ PolicyId::kMinorAllowance, DEMOCRATIC_MINOR_ALLOWANCE });
	targets.push_back({ PolicyId::kImmigrationRatio, DEMOCRATIC_IMMIGRATION });
}

PartyCongress DemocraticParty::GetCongress(int year, int month) {
	(void)year;
	(void)month;
	// 调试用：每个月1号12:00到15:00开党代会。
	PartyCongress congress;
	congress.day = 1;
	congress.start = Time(1, 1, 1, 12, 0, 0);
	congress.end = Time(1, 1, 1, 15, 0, 0);
	return congress;
}

void DemocraticParty::ChooseMeetingRoom(PostHandle* post) {
	meetingAddress = PickRandomMeetingRoom(post);
}

void DemocraticParty::AssignPowers(const PartyContext& context) {
	AssignPolicyPowersRandomly(context, assignments);
}
