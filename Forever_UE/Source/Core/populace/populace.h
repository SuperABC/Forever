#pragma once

#include "class.h"

#include "populace/name_factory.h"
#include "populace/scheduler_factory.h"
#include "story/script_factory.h"
#include "common/utility.h"
#include "common/handle.h"

#include <functional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

struct ScriptContext;

// Populace：和Map平级的顶层Core类，不是挂在Map底下的工具函数集合——自己管理Citizen*的
// 生命周期，不知道Map的存在(和老工程Populace/Map::Checkin(populace, player)同一个解耦
// 方向)，详见Source/Core/populace/populace.md。
class Populace {
public:
	Populace();
	~Populace();

	// 复刻老工程Populace::Init：accommodation由调用方从Map::ComputeAccommodationTarget()
	// 算好传进来(Populace不知道Map的存在)。先发现/注册Name mod dll(InitNames)，再对
	// accommodation做一次老工程同款的扰动(target = accommodation *
	// exp(GetRandom(1000)/1000.0f - 0.5f))得到真正的模拟目标，最后跑一次年表模拟，把存活
	// 个体物化成Citizen存进citizens。
	void Init(int accommodation);

	const std::vector<Citizen*>& GetCitizens() const;

	// 驱动Job的DailyPlan/ExecNode调度（Organization那一份调度在Society::Tick里，两套
	// 独立的timer，见populace.md"两套独立timer"一节）：crossedDay时给每个持有job的
	// citizen生成今天的调度、塞进jobTimerSet；不论是否crossedDay，每帧从jobTimerSet
	// 弹出最多kMaxJobTimersPerTick个到期节点执行。回调带Citizen*——调用方
	// (AForeverFrameworkActor::Tick)要按这个Citizen*决定NPCNavigateChange具体怎么
	// 生效（找它当前是否有已生成的ACitizenElement），Populace自己不知道Actor层。
	// @post：透传给Job::DailyPlan/ExecNode，再转发到JobMod，供mod按需查citizen家/工位
	// 的具体地址（见Dependence/society/job_mod.h），Populace自己不解读这个句柄。
	void Tick(const Time& currentTime, bool crossedDay,
		const std::function<void(Citizen*, const std::vector<Change*>&)>& onActions, PostHandle* post);

	// 阶段占位：目前没有任何Change子类是Populace域自己认识、需要处理的，空实现——
	// AForeverFrameworkActor::ApplyChange会把同一个Change转发给全部六个域，这里不打"未实现"
	// 警告（避免同一个Change被六个域各打一遍重复警告），唯一的兜底警告在Story::ApplyChange。
	void ApplyChange(const Change* change, const ScriptContext& context);

	// 按姓名线性查找Citizen*——不强制生成/持有任何Actor，纯Core层数据查询。供
	// AForeverFrameworkActor::ApplyChange处理NPCNavigateChange时，用change自带的occupant
	// 姓名反查Citizen*（原来的Tick回调直接拿到Citizen*形参，改成统一的
	// ApplyChange(const Change*, const ScriptContext&)签名后不再有实体指针，只能反过来按名字
	// 查，和UForeverPopulaceFrameworkComponent::FindOrSpawnCitizenByName是同一个思路，一个在
	// Core层查数据，一个在UE层顺带生成Actor）。找不到返回nullptr。
	Citizen* FindCitizenByName(const std::string& name) const;

	// 模拟结束时的"当前年份"(老工程time->SetYear(year+2000)那个值)——Map::Checkin()用它
	// 给Citizen::GetAge()算成年/未成年。Player的全局时钟落地后，
	// AForeverFrameworkActor::EnsurePlayerGenerated()也用这个值把开局时间设成这一年的
	// 1月1日8点(player->SetTime(Time(GetCurrentYear(), 1, 1, 8))，见player.md)。
	// Init()跑完之前返回默认值2000。
	int GetCurrentYear() const;

private:
	// 创建"chinese"这个具体实现的Name概念实例——name mod dll的发现/注册不在这里做，属于
	// `Registry`全局一次性注册的范围(见Source/Core/common/registry.md)，nameFactory已经是
	// 绑定好的引用成员，这里只用它创建实例。Populace自己只操作Core层的Name概念，不直接
	// 持有/调用NameMod*，见Source/Core/populace/populace.md、Source/Core/populace/name.md。
	void InitNames();

	// 复刻老工程Populace::GenerateCitizens的年表模拟算法，见populace.cpp/populace.md。
	// 函数体末尾顺带生成亲属关系(GenerateKinshipRelations，含配偶/子女/兄弟姐妹)——需要
	// 用到模拟阶段的Human数据(结婚年份/父母索引)，这些数据物化成Citizen后就丢弃了，只能
	// 在这个函数体内、Human数组还在作用域内时处理，见populace.md"四类人际关系生成"一节。
	void GenerateCitizens(int target);

	// 情感关系(恋爱/婚姻/情人)历史生成——GenerateCitizens()之后调用，读取
	// GenerateCitizens()已经生成好的KinshipExperience(RELATIVE_SPOUSE)::GetBeginYear()
	// 当结婚年份用，不重新计算，见populace.md同一节。
	void GenerateRomanticRelations();

	// 虚拟学校/班级 + 同学关系生成——按年份正序推进的编年模拟，见populace.md同一节。
	void GenerateEducations();

	// 给每个citizen加权随机分配一个Scheduler——参考老工程同名算法(累加所有已注册Scheduler
	// 类型的权重建CDF，对每个citizen roll一个随机数选中一个类型)，权重来源改成当前工程
	// 已经确立的SchedulerFactory::GetPower(id)单个查询写法(和Society::Init选Organization
	// 类型同一套CDF算法)，不照抄老工程SchedulerFactory::GetPowers()一次性返回全部map的
	// 写法。GenerateCitizens()跑完、citizens列表已经就绪之后调用一次，见populace.md。
	void AssignSchedulers();

	// 引用`Registry::Get().GetNameFactory()`，不再自己持有ModLoader/NameFactory——mod dll
	// 的发现/注册只在整个UE进程生命周期里跑一次，见Source/Core/common/registry.md。构造函数
	// 初始化列表里绑定。
	NameFactory& nameFactory;
	Name* name = nullptr; // InitNames()创建，~Populace()里delete(Name析构会调用nameFactory.DestroyName())

	// 引用`Registry::Get().GetSchedulerFactory()`/`GetScriptFactory()`，和nameFactory
	// 同一个模式——scriptFactory是每个Scheduler独占的Script（见scheduler.h）需要的构造
	// 参数，Populace这次第一次需要依赖Story域的ScriptFactory，和Society持有
	// ScriptFactory引用成员（给Job/Organization各自的Script用）是同一个先例。
	SchedulerFactory& schedulerFactory;
	ScriptFactory& scriptFactory;

	std::vector<Citizen*> citizens;
	int currentYear = 2000;

	// 虚拟学校/班级——没有Mod/Factory背书的纯Core实体，GenerateEducations()生成，
	// ~Populace()统一delete，见school.h。
	std::vector<SchoolClass*> schoolClasses;

	std::set<std::tuple<Time, Citizen*, std::string>> jobTimerSet;
	static constexpr int kMaxJobTimersPerTick = 1;

	// Scheduler自己独立的一套timer，和jobTimerSet完全平行（见Tick()实现），负责citizen
	// 下班之后的行为。
	std::set<std::tuple<Time, Citizen*, std::string>> schedulerTimerSet;
	static constexpr int kMaxSchedulerTimersPerTick = 1;
};
