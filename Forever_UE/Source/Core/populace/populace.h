#pragma once

#include "class.h"

#include "populace/name_factory.h"
#include "populace/scheduler_factory.h"
#include "story/script_factory.h"
#include "common/utility.h"
#include "common/handle.h"
#include "populace/citizen.h"

#include <climits>
#include <functional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

// Populace：和Map平级的顶层Core类，不是挂在Map底下的工具函数集合——自己管理Citizen*的
// 生命周期，不知道Map的存在，详见Source/Core/populace/populace.md。
class Populace {
	// 存档：PopulaceSaveCodec直接读写私有字段(populace_save.md)。
	friend class PopulaceSaveCodec;
public:
	Populace();
	~Populace();

	// accommodation来自Map::ComputeAccommodationTarget()，workCapacity来自Map::ComputeWorkCapacity()
	// (扰动后的目标人数只封顶)。流程见populace.md。
	void Init(int accommodation, int workCapacity = INT_MAX);

	const std::vector<Citizen*>& GetCitizens() const;

	// 驱动饱食度衰减和Job/Scheduler两套定时器，见populace.md"Tick"。onActions回调带Citizen*，
	// post原样透传给Job/Scheduler。
	// 是否还有已到时(<=currentTime)但没处理的Job/Scheduler timer(每次Tick各有条数上限)。跳时间用：
	// 反复Tick同一时刻直到它为false。
	bool HasDueTimers(const Time& currentTime) const;

	void Tick(const Time& currentTime, bool crossedDay,
		const std::function<void(Citizen*, const std::vector<Change*>&)>& onActions, PostHandle* post);

	// 处理add_option/remove_option。返回是不是本域认识的类型(认识就true，找不到目标静默忽略)。
	bool ApplyChange(const Change* change, const ScriptContext& context);

	// 按姓名线性查找，找不到返回nullptr。
	Citizen* FindCitizenByName(const std::string& name) const;

	// 运行中新增市民(spawn_npc)，见populace.md。重名返回nullptr。
	Citizen* SpawnCitizen(const std::string& name, GENDER_TYPE gender, int birthYear, int birthMonth, int birthDay,
		const std::string& schedulerId, const std::string& avatar = "");

	// 运行中迁入一位成年新市民（Immigration用）：随机性别、随机姓名（重名重试）、20~45岁随机生日、
	// 随机日程类型，不设住处/积蓄/工作。姓名一直撞车时返回nullptr。
	Citizen* SpawnImmigrant(int currentYear);

	// 模拟结束时的"当前年份"，Init()跑完之前返回默认值2000。
	int GetCurrentYear() const;
	// 跳时间跨年时由框架同步(年龄=currentYear-出生年)，随人口段存档。
	void SetCurrentYear(int year);
	// 开局（Init跑完）时的市民总数，迁入机制以它为基准。状态，随人口段存档。
	int GetInitialPopulation() const { return initialPopulation; }

private:
	// 按config选取名算法创建Name，并登记剧情占位名，见populace.md。
	void InitNames();

	// 年表模拟，末尾顺带生成亲属关系(需要模拟阶段的Human数据)，见populace.md。
	void GenerateCitizens(int target);

	// 情感关系(恋爱/婚姻/情人)历史生成，见populace.md。
	void GenerateRomanticRelations();

	// 虚拟学校/班级 + 同学关系生成，见populace.md。
	void GenerateEducations();

	// 给每个citizen加权随机分配一个Scheduler，见populace.md。
	void AssignSchedulers();
	// 按SchedulerFactory::GetPower加权随机选一个Scheduler类型，没有注册任何类型时返回空串。
	std::string PickSchedulerId() const;

	// 引用Registry里的全局Factory(见common/registry.md)，构造函数初始化列表里绑定。
	NameFactory& nameFactory;
	Name* name = nullptr; // InitNames()创建，~Populace()里delete(Name析构会调用nameFactory.DestroyName())

	// scriptFactory是每个Scheduler独占的Script需要的构造参数。
	SchedulerFactory& schedulerFactory;
	ScriptFactory& scriptFactory;

	std::vector<Citizen*> citizens;
	int currentYear = 2000;
	int initialPopulation = 0;

	// 虚拟学校/班级，GenerateEducations()生成，~Populace()统一delete。
	std::vector<SchoolClass*> schoolClasses;

	std::set<std::tuple<Time, Citizen*, std::string>> jobTimerSet;
	static constexpr int kMaxJobTimersPerTick = 16;

	// 上一次Tick的游戏时间，用来算饱食度衰减的经过时间；不存档。
	Time lastTickTime;
	bool hasLastTickTime = false;

	// Scheduler独立的一套timer，和jobTimerSet平行。
	std::set<std::tuple<Time, Citizen*, std::string>> schedulerTimerSet;
	static constexpr int kMaxSchedulerTimersPerTick = 16;
};
