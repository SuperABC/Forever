#pragma once

#include "common/mod_state.h"
#include "common/utility.h"
#include "common/handle.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class Change;

// 组织对外提供的一项服务：服务名只给组织自己后面实际提供服务时识别，类型/单价/质量给系统撮合。
struct ServiceOffer {
	std::string name;
	std::string type;
	int64_t unitPrice = 0; // 分
	float quality = 0.f;
	int capacity = 0;      // 当日最多能提供的份数（商店是该货物的库存），系统按它加权分配需求、防止超卖
};

// 设计见organization_mod.md。Dependence层看不到Job/Component/Room/Citizen,接口只用字符串/int/Time/Change*。
class OrganizationMod {
public:
	OrganizationMod() = default;
	virtual ~OrganizationMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 子类还必须实现static GetPower()和static GetRequirements(),通过RegisterOrganization注册,见organization_mod.md。

	// 每个workspace房间调一次,把要配的job类型字符串push进this->vacancies。
	// 必须在方法开头自己clear:Core不清空(string在mod堆上分配,Core侧释放会跨DLL堆崩溃)。
	virtual void DesignJobsForRoom(const std::string& componentType, const std::string& componentName,
		const std::string& roomType, const std::string& roomName, int workspaceCapacity) = 0;

	// 每日服务收集:系统先把状态写进script(商店是self.stock.<货物类型>),stockTypes列出写入的类型,
	// mod在services里填本日能提供的服务(先clear再push)。post可查商品零售价等(post "product info")。
	virtual void CollectServices(Container* script, const std::vector<std::string>& stockTypes,
		PostHandle* post) {}

	// 市民用掉了count份name服务:mod改script变量,系统随后回写Storage。
	virtual void ApplyServiceUsage(const std::string& name, int count, Container* script) {}

	virtual void DailyPlan(const Time& currentTime, PostHandle* post) {}
	virtual void ExecNode(const std::string& node, PostHandle* post) {}

	std::vector<std::string> vacancies;
	std::vector<ServiceOffer> services; // CollectServices的输出

	// 经济参数(分),语义见organization_mod.md。
	int64_t initialFunds = 0;
	std::vector<float> shareholders;
	int64_t wage = 0;
	std::unordered_map<std::string, Time> plans;
	std::vector<Change*> changes;

	// Script配置,同JobMod。
	std::string scriptModName; // 空=无自定义ScriptMod；和milestoneNames都空则实例没有Script(GetScript()==nullptr)，见MakeScript
	std::vector<std::string> milestoneNames;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
