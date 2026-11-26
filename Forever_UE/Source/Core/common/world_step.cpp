#include "world_step.h"

#include "common/error.h"

#include "common/implement.h"
#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "populace/populace.h"
#include "story/change.h"
#include "populace/citizen.h"
#include "society/society.h"
#include "society/immigration.h"
#include "industry/industry.h"
#include "traffic/traffic.h"
#include "story/story.h"
#include "player/player.h"
#include "polit/polit.h"


using namespace std;

void WorldStep::TickPopulace(const WorldDomains& domains, bool crossedDay, PostImplement& post, const WorldStepHooks& hooks) {
	if (!domains.populace || !domains.player) return;
	ErrorScope stepScope(kErrorScopeStep, "tick.populace");
	const auto noop = [](Citizen*, const vector<Change*>&) {};
	domains.populace->Tick(*domains.player->GetTime(), crossedDay,
		hooks.onCitizenChanges ? hooks.onCitizenChanges : function<void(Citizen*, const vector<Change*>&)>(noop), &post, hooks.controlledCitizen);

	// 瞌睡值掉到0的市民已经被Populace强制睡下(只有玩家正在操控的市民会扣瞌睡值，所以最多就他一个)。
	// 世界要跟着跳过这段时间：交给调用方执行一条player_sleep(UE层据此跳时间)。
	for (Citizen* sleeper : domains.populace->ConsumeForcedSleepers()) {
		if (sleeper != hooks.controlledCitizen || !hooks.onCitizenChanges) continue;
		PlayerSleepChange forced(std::to_string(Citizen::kForcedSleepHours));
		hooks.onCitizenChanges(sleeper, { &forced });
	}
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

	if (domains.map) {
		ErrorScope stepScope(kErrorScopeStep, "tick.map");
		domains.map->Tick(now, crossedDay, &post);
	}

	TickPopulace(domains, crossedDay, post, hooks);
	TickSociety(domains, crossedDay, post, hooks);

	if (domains.industry && domains.society) Economy::SyncIndustry(*domains.society, *domains.industry); // 倒闭企业的房间停业
	if (domains.industry) {
		ErrorScope stepScope(kErrorScopeStep, "tick.industry");
		domains.industry->Tick(now, crossedDay, &post);
	}
	if (domains.industry && domains.society && domains.map && domains.populace && crossedDay) {
		ErrorScope stepScope(kErrorScopeStep, "daily.economy");
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
	// 政治域每步都推进(不受crossedDay限制)：党代会按小时级时间窗口走。
	if (domains.polit) {
		ErrorScope stepScope(kErrorScopeStep, "tick.polit");
		domains.polit->Step(now);
		if (hooks.onPoliticsTicked) hooks.onPoliticsTicked();
	}
	if (domains.traffic) {
		ErrorScope stepScope(kErrorScopeStep, "tick.traffic");
		domains.traffic->Tick(now);
		if (hooks.onTrafficTicked) hooks.onTrafficTicked();
	}
	if (domains.story) {
		ErrorScope stepScope(kErrorScopeStep, "tick.story");
		domains.story->Tick(now, crossedDay, &post);
	}
	return report;
}
