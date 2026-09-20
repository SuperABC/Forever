#pragma once

#include "common/utility.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class Change;

// OrganizationMod：一种组织类型的行为定义(比如商店)。**Dependence层看不到Job/Component/
// Room/Citizen这些Core类**（Core依赖Dependence，反过来不行——照抄BuildingMod::Layout/
// Assign只出现Quad/Road/Lot这几个Dependence层类型的既有模式），所以这里的接口只用
// 字符串/int/Time/Change*表达"要什么""配什么"，真正的Component/Room遍历和new Job(...)
// 全部在Core侧的Organization类里做，见Source/Core/society/organization.md。
class OrganizationMod {
public:
	OrganizationMod() = default;
	virtual ~OrganizationMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// static float GetPower(); // 通过OrganizationFactory::RegisterOrganization的
	// PowerFunc函数指针注册——和BuildingMod::GetPower同一个模式，基础值，不需要实例，
	// Society::Init要对着"每个Component实例 x 每种已注册组织类型"做加权判断，不能为
	// 每个组合都构造一个实例。

	// 写进this->requirements：这个组织类型需要哪些Component类型(字符串)+每种数量
	// 范围[min,max]（可以同时列多种不同类型，也可以对同一种类型要多个）。
	virtual void ComponentRequirements() = 0;

	// 写进this->vacancies：Core侧Organization自己遍历claimed components的每个
	// workspace room，每个room调用一次这个方法（调用前Core会先清空vacancies），把这个
	// room的完整上下文喂给mod——component类型/名字、room类型/名字、这个room的工位数量
	// (WorkspaceCapacity())。mod自己决定要在这个room里配几个、什么类型的Job(可以是
	// 不同类型混着配)，把job类型字符串逐条push进this->vacancies；Core侧读完vacancies，
	// 对每一条按顺序new Job(...)，position都设成这次传进来的room。这个room不需要配
	// 任何Job就不push。
	virtual void DesignJobsForRoom(const std::string& componentType, const std::string& componentName,
		const std::string& roomType, const std::string& roomName, int workspaceCapacity) = 0;

	// 可选：这次大部分组织不需要自己的调度(比如发工资)，默认空实现——真要用的具体组织
	// 类型重写，直接写自己的plans/changes成员字段，和JobMod同一个模式。
	virtual void DailyPlan(const Time& currentTime) {}
	virtual void ExecNode(const std::string& node) {}

	std::unordered_map<std::string, std::pair<int, int>> requirements;
	std::vector<std::string> vacancies;
	std::unordered_map<std::string, Time> plans;
	std::vector<Change*> changes;
};
