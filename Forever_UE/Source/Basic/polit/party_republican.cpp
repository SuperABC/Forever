#include "party_republican.h"

#include "party_common.h"
#include "polit/policy_ids.h"

using namespace std;

// ---- 占位初值(等用户调)：目标值，Core会换算成相对当前值的比例并夹到[0.9, 1.1] ----
// 税率单位%，补助单位分/天，移民目标比例=目标人口/开局人口。
#define REPUBLICAN_MEMBERS 12 // 党员人数
#define REPUBLICAN_CORPORATE_TAX 50
#define REPUBLICAN_INCOME_TAX 30
#define REPUBLICAN_CONSUMPTION_TAX 15
#define REPUBLICAN_RENT_TAX 30
#define REPUBLICAN_IMPORT_TARIFF 40
#define REPUBLICAN_PENSION 8000
#define REPUBLICAN_UNEMPLOYMENT 8000
#define REPUBLICAN_MINOR_ALLOWANCE 3000
#define REPUBLICAN_IMMIGRATION 1.2

int RepublicanParty::count = 0;

RepublicanParty::RepublicanParty() : id(count++) {
}

const char* RepublicanParty::GetName() {
	name = "RepublicanParty" + to_string(id);
	return name.data();
}

void RepublicanParty::SetProperty() {
	memberCount = REPUBLICAN_MEMBERS;
}

void RepublicanParty::Propose(const vector<PolicyValue>& current) {
	(void)current; // 固定目标值：不看当前值，永远提议同一组数
	targets.clear();
	targets.push_back({ PolicyId::kCorporateTax, REPUBLICAN_CORPORATE_TAX });
	targets.push_back({ PolicyId::kIncomeTax, REPUBLICAN_INCOME_TAX });
	targets.push_back({ PolicyId::kConsumptionTax, REPUBLICAN_CONSUMPTION_TAX });
	targets.push_back({ PolicyId::kRentTax, REPUBLICAN_RENT_TAX });
	targets.push_back({ PolicyId::kImportTariff, REPUBLICAN_IMPORT_TARIFF });
	targets.push_back({ PolicyId::kRetirementPension, REPUBLICAN_PENSION });
	targets.push_back({ PolicyId::kUnemploymentBenefit, REPUBLICAN_UNEMPLOYMENT });
	targets.push_back({ PolicyId::kMinorAllowance, REPUBLICAN_MINOR_ALLOWANCE });
	targets.push_back({ PolicyId::kImmigrationRatio, REPUBLICAN_IMMIGRATION });
}

PartyCongress RepublicanParty::GetCongress(int year, int month) {
	(void)year;
	(void)month;
	// 调试用：每个月1号12:00到15:00开党代会。
	PartyCongress congress;
	congress.day = 1;
	congress.start = Time(1, 1, 1, 12, 0, 0);
	congress.end = Time(1, 1, 1, 15, 0, 0);
	return congress;
}

void RepublicanParty::ChooseMeetingRoom(PostHandle* post) {
	meetingAddress = PickRandomMeetingRoom(post);
}

void RepublicanParty::AssignPowers(const PartyContext& context) {
	AssignPolicyPowersRandomly(context, assignments);
}
