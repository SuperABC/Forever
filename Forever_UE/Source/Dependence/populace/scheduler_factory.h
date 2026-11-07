#pragma once

#include "populace/scheduler_mod.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>


class SchedulerFactory {
public:
	using CreateFunc = SchedulerMod*(*)(const std::string&);
	using DestroyFunc = void(*)(SchedulerMod*);
	using PowerFunc = float(*)();

	SchedulerFactory() = default;
	virtual ~SchedulerFactory() = default;

	virtual void RegisterScheduler(const std::string& id, CreateFunc creator, DestroyFunc deleter, PowerFunc power);

	virtual SchedulerMod* CreateScheduler(const std::string& id);

	// 转发调用注册时提供的static函数，不需要任何SchedulerMod实例存在。id未注册时返回0.f。
	// 供Populace::AssignSchedulers()建CDF做加权随机分配用，和OrganizationFactory::
	// GetPower同一个模式。
	virtual float GetPower(const std::string& id) const;

	// 必须走注册时mod提供的deleter释放,不能Factory直接delete——跨DLL new/delete安全,
	// 通过liveInstances反查实例对应的注册id、再取出对应deleter调用。
	virtual void DestroyScheduler(SchedulerMod* instance);

	virtual bool CheckRegistered(const std::string& id) const;
	virtual std::vector<std::string> GetRegisteredIds() const;

	// 当前存活的实例和它们的注册id——存档收集各mod的命名计数器/状态版本用。
	virtual std::vector<std::pair<std::string, SchedulerMod*>> GetLiveInstances() const;

	// 设置这个concept从config.json"scheduler_mods"数组解析出的(id, 参数字符串)表,
	// 必须在调用方触发mod的RegisterModSchedulers导出函数之前调用,否则CreateScheduler
	// 拿实例创建时查不到对应参数。
	virtual void SetModArgs(const std::unordered_map<std::string, std::string>& argsById);

private:
	// 启用=已注册且id在configuredArgs里,详见配对md。
	bool IsEnabled(const std::string& id) const;

	struct Entry {
		CreateFunc creator;
		DestroyFunc deleter;
		PowerFunc power;
	};

	std::unordered_map<std::string, Entry> registries;
	std::unordered_map<SchedulerMod*, std::string> liveInstances;
	std::unordered_map<std::string, std::string> configuredArgs;
};
