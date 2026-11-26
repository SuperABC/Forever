#include "organization_video_company.h"

#include "organization_common.h"

#include "common/json.h"

#include <algorithm>

using namespace std;

namespace {
	constexpr int64_t kVideoPrice = 1000;     // 每份视频10元(分)
	constexpr float kVideoQuality = 0.1f;
	constexpr int kVideosPerWorkerPerDay = 20; // 每个在岗视频制作员每天能做的份数
}

int VideoCompanyOrganization::count = 0;

VideoCompanyOrganization::VideoCompanyOrganization() : id(count++) {
	milestoneNames = { "organization_video_company" };

	initialFunds = 20000000;         // 20万元
	shareholders = vector<float>(10, 0.1f); // 10个股东各占10%
	dividendPayoutRatio = 0.5f;             // 税后利润分一半，其余留在企业账户
	wage = 15000;                    // 每个员工每日150元
}

const char* VideoCompanyOrganization::GetName() {
	name = "VideoCompanyOrganization" + to_string(id);
	return name.data();
}

const unordered_map<string, pair<int, int>>& VideoCompanyOrganization::GetRequirements() {
	static const unordered_map<string, pair<int, int>> requirements = { { "component_office", { 1, 2 } } };
	return requirements;
}

void VideoCompanyOrganization::DesignJobsForRoom(const std::string& componentType, const std::string&,
	const std::string& roomType, const std::string& roomName, int workspaceCapacity, bool reset) {
	if (reset) vacancies.clear(); // 必须由mod自己清空，见organization_mod.h
	// 按房间容纳量满编定员。
	if (componentType != "component_office" || roomType != "room_office") return;
	for (int i = 0; i < workspaceCapacity; i++) vacancies.push_back({ roomName, "job_video_maker" });
}

void VideoCompanyOrganization::MonthlySettle(const OrganizationMonthlyState& state) {
	issueShares = DistressIssue(state);
	// 视频制作员跟着利润扩缩（每个房间满编到2倍之间，裁到一半为止）。
	int makers = PlannedCount(vacancies, "job_video_maker");
	int delta = HeadcountDelta(state, makers, wage);
	if (delta != 0) AdjustPlan(vacancies, state, "job_video_maker", delta, 50, 200);
}

void VideoCompanyOrganization::CollectServices(Container* script, const vector<string>&, PostHandle*) {
	services.clear();
	if (!script) return;
	// 系统每天把在岗人数写进self.employees；产能=在岗视频制作员数x每人每天份数。
	const ValueType* value = script->FindValue("employees");
	int employees = value ? ToInt(*value) : 0;
	if (employees <= 0) return;

	ServiceOffer offer;
	offer.name = "video";
	offer.type = "video";
	offer.unitPrice = kVideoPrice;
	offer.quality = kVideoQuality;
	offer.capacity = employees * kVideosPerWorkerPerDay;
	services.push_back(offer);
}
