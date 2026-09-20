#pragma once

#include "class.h"

#include "populace/name_factory.h"

#include <vector>


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
	void GenerateCitizens(int target);

	// 引用`Registry::Get().GetNameFactory()`，不再自己持有ModLoader/NameFactory——mod dll
	// 的发现/注册只在整个UE进程生命周期里跑一次，见Source/Core/common/registry.md。构造函数
	// 初始化列表里绑定。
	NameFactory& nameFactory;
	Name* name = nullptr; // InitNames()创建，~Populace()里delete(Name析构会调用nameFactory.DestroyName())

	std::vector<Citizen*> citizens;
	int currentYear = 2000;
};
