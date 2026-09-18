#pragma once

#include "common/loader.h"
#include "populace/name_factory.h"

#include <vector>

class Citizen;
class Name;

// Populace：和Map平级的顶层Core类，不是挂在Map底下的工具函数集合——自己管理Citizen*的
// 生命周期，不知道Map的存在(和老工程Populace/Map::Checkin(populace, player)同一个解耦
// 方向)，详见Source/Core/populace/populace.md。
class Populace {
public:
	Populace() = default;
	~Populace();

	// 复刻老工程Populace::Init：accommodation由调用方从Map::ComputeAccommodationTarget()
	// 算好传进来(Populace不知道Map的存在)。先发现/注册Name mod dll(InitNames)，再对
	// accommodation做一次老工程同款的扰动(target = accommodation *
	// exp(GetRandom(1000)/1000.0f - 0.5f))得到真正的模拟目标，最后跑一次年表模拟，把存活
	// 个体物化成Citizen存进citizens。
	void Init(int accommodation);

	const std::vector<Citizen*>& GetCitizens() const;

	// 模拟结束时的"当前年份"(老工程time->SetYear(year+2000)那个值)——Map::Checkin()用它
	// 给Citizen::GetAge()算成年/未成年，不需要真正的日历/游戏时钟系统。Init()跑完之前
	// 返回默认值2000。
	int GetCurrentYear() const;

private:
	// 用ModLoader发现/注册config.json配置的name mod dll，创建"chinese"这个具体实现的
	// Name概念实例——和Map::InitBuildings()等发现/注册mod dll的写法同一个模式，这次Populace自己
	// 独立持有一份ModLoader/NameFactory(不和Map共用，两者互不知道对方存在)。Populace自己
	// 只操作Core层的Name概念，不直接持有/调用NameMod*，见Source/Core/populace/populace.md、
	// Source/Core/populace/name.md。
	void InitNames();

	// 复刻老工程Populace::GenerateCitizens的年表模拟算法，见populace.cpp/populace.md。
	void GenerateCitizens(int target);

	ModLoader modLoader;
	NameFactory nameFactory;
	Name* name = nullptr; // InitNames()创建，~Populace()里delete(Name析构会调用nameFactory.DestroyName())

	std::vector<Citizen*> citizens;
	int currentYear = 2000;
};
