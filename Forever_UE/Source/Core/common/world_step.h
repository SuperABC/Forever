#pragma once

#include "class.h"

#include "common/implement.h"
#include "society/economy.h"

#include <functional>
#include <vector>


// 一局世界里的全部Core域(没生成的域为空指针，对应步骤跳过)。
struct WorldDomains {
	Map* map = nullptr;
	Populace* populace = nullptr;
	Society* society = nullptr;
	Industry* industry = nullptr;
	Traffic* traffic = nullptr;
	Story* story = nullptr;
	Player* player = nullptr;
	Polit* polit = nullptr;
};

// 调用方(UE层/测试程序)注入的、Core看不到的那部分：怎么应用脚本产出的Change、离城/迁入的市民怎么处理。
struct WorldStepHooks {
	std::function<void(Citizen*, const std::vector<Change*>&)> onCitizenChanges;
	std::function<void(Organization*, const std::vector<Change*>&)> onOrganizationChanges;
	// 玩家当前控制的市民：他所在的户不会被判离城。
	const Citizen* controlledCitizen = nullptr;
	// 本月整户离城的市民(Economy::Daily产出)。
	std::function<void(Citizen*)> onDeparted;
	// 城外迁入的新市民(Immigration::Daily产出)。
	std::function<void(Citizen*)> onImmigrated;
	// Traffic::Tick之后、Story::Tick之前：UE层在这里消费公交停站事件，转发给载具脚本。
	std::function<void()> onTrafficTicked;
	// Polit::Step之后：UE层在这里取党代会开始/结束通知(Polit::ConsumeMeetingNotices)，转发给党员的脚本。
	std::function<void()> onPoliticsTicked;
};

// 世界按时间推进一步的唯一实现，UE的AForeverFrameworkActor::StepWorld和测试程序共用，见world_step.md。
class WorldStep {
public:
	// 玩家时钟(domains.player)必须已经推进到本步的时间。crossedDay=本步是否跨过0点。
	// 返回当天的经济日报；没跑经济链(没跨天或缺域)时返回的日报是默认值，economyRan为false。
	static DailyReport Step(const WorldDomains& domains, bool crossedDay, const WorldStepHooks& hooks, bool* economyRan = nullptr);

	// 单独Tick人口/社会域，跳时间排空到期计时器时反复调用。
	static void TickPopulace(const WorldDomains& domains, bool crossedDay, PostImplement& post, const WorldStepHooks& hooks);
	static void TickSociety(const WorldDomains& domains, bool crossedDay, PostImplement& post, const WorldStepHooks& hooks);
};
