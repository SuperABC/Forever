#pragma once

#include "society/job_mod.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>


class JobFactory {
public:
	using CreateFunc = JobMod*(*)(const std::string&);
	using DestroyFunc = void(*)(JobMod*);

	JobFactory() = default;
	virtual ~JobFactory() = default;

	virtual void RegisterJob(const std::string& id, CreateFunc creator, DestroyFunc deleter);

	virtual JobMod* CreateJob(const std::string& id);

	// 必须走注册时mod提供的deleter释放,不能Factory直接delete——跨DLL new/delete安全,
	// 通过liveInstances反查实例对应的注册id、再取出对应deleter调用。
	virtual void DestroyJob(JobMod* instance);

	virtual bool CheckRegistered(const std::string& id) const;
	virtual std::vector<std::string> GetRegisteredIds() const;

	// 当前存活的实例和它们的注册id——存档收集各mod的命名计数器/状态版本用。
	virtual std::vector<std::pair<std::string, JobMod*>> GetLiveInstances() const;

	// 设置这个concept从config.json"job_mods"数组解析出的(id, 参数字符串)表,
	// 必须在调用方触发mod的RegisterModJobs导出函数之前调用,否则CreateJob
	// 拿实例创建时查不到对应参数。
	virtual void SetModArgs(const std::unordered_map<std::string, std::string>& argsById);

private:
	// 启用=已注册且id在configuredArgs里,详见配对md。
	bool IsEnabled(const std::string& id) const;

	struct Entry {
		CreateFunc creator;
		DestroyFunc deleter;
	};

	std::unordered_map<std::string, Entry> registries;
	std::unordered_map<JobMod*, std::string> liveInstances;
	std::unordered_map<std::string, std::string> configuredArgs;
};
