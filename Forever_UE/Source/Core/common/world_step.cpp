#include "world_step.h"

#include "common/implement.h"
#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "populace/populace.h"
#include "society/society.h"
#include "society/immigration.h"
#include "industry/industry.h"
#include "traffic/traffic.h"
#include "story/story.h"
#include "player/player.h"


using namespace std;

void WorldStep::TickPopulace(const WorldDomains& domains, bool crossedDay, PostImplement& post, const WorldStepHooks& hooks) {
	if (!domains.populace || !domains.player) return;
	const auto noop = [](Citizen*, const vector<Change*>&) {};
	domains.populace->Tick(*domains.player->GetTime(), crossedDay,
		hooks.onCitizenChanges ? hooks.onCitizenChanges : function<void(Citizen*, const vector<Change*>&)>(noop), &post);
}

void WorldStep::TickSociety(const WorldDomains& domains, bool crossedDay, PostImplement& post, const WorldStepHooks& hooks) {
	if (!domains.society || !domains.player) return;
	const auto noop = [](Organization*, const vector<Change*>&) {};
	domains.society->Tick(*domains.player->GetTime(), crossedDay,
		hooks.onOrganizationChanges ? hooks.onOrganizationChanges : function<void(Organization*, const vector<Change*>&)>(noop), &post);
}

DailyReport WorldStep::Step(const WorldDomains& domains, bool crossedDay, const WorldStepHooks& hooks, bool* economyRan) {
	DailyReport report;
	if (economyRan) *economyRan = false;
	if (!domains.player) return report;
	const Time& now = *domains.player->GetTime();

	// 年份变了就同步给人口(年龄=currentYear-出生年)，要在各域Tick之前，它们当天就用新年龄。
	if (domains.populace && now.GetYear() != domains.populace->GetCurrentYear()) {
		domains.populace->SetCurrentYear(now.GetYear());
	}

	// 现场构造、只覆盖这一步的PostImplement(Job/OrganizationMod通过Post()查地址，见Core/common/implement.cpp)。
	PostImplement post(domains.map, domains.populace, domains.society, domains.story, domains.industry, domains.traffic, domains.player);

	if (domains.map) domains.map->Tick(now, crossedDay, &post);

	TickPopulace(domains, crossedDay, post, hooks);
	TickSociety(domains, crossedDay, post, hooks);

	if (domains.industry && domains.society) Economy::SyncIndustry(*domains.society, *domains.industry); // 倒闭企业的房间停业
	if (domains.industry) domains.industry->Tick(now, crossedDay, &post);
	if (domains.industry && domains.society && domains.map && domains.populace && crossedDay) {
		// 每日经济驱动链：货款、服务买卖、房租、工资、补助，月初做月度分红，见Core/society/economy.md。
		vector<Room*> allRooms;
		for (const auto& [name, building] : domains.map->GetBuildings()) {
			if (!building) continue;
			for (Room* room : building->GetRooms()) allRooms.push_back(room);
		}
		report = Economy::Daily(*domains.society, *domains.industry, allRooms, domains.populace->GetCitizens(), now, &post,
			hooks.controlledCitizen);
		if (economyRan) *economyRan = true;
		if (hooks.onDeparted) for (Citizen* departed : report.departed) hooks.onDeparted(departed);
		for (Citizen* immigrant : Immigration::Daily(*domains.society, *domains.populace, *domains.map, now)) {
			if (hooks.onImmigrated) hooks.onImmigrated(immigrant);
		}
	}
	if (domains.traffic) {
		domains.traffic->Tick(now, crossedDay, &post);
		if (hooks.onTrafficTicked) hooks.onTrafficTicked();
	}
	if (domains.story) domains.story->Tick(now, crossedDay, &post);
	return report;
}
