#pragma once

#include "society/organization_mod.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>


class OrganizationFactory {
public:
	using CreateFunc = OrganizationMod*(*)(const std::string&);
	using DestroyFunc = void(*)(OrganizationMod*);
	using PowerFunc = float(*)();
	// 返回mod内部static变量的const引用(调用方只读)，不跨DLL传递容器所有权。
	using RequirementsFunc = const std::unordered_map<std::string, std::pair<int, int>>&(*)();

	OrganizationFactory() = default;
	virtual ~OrganizationFactory() = default;

	virtual void RegisterOrganization(const std::string& id, CreateFunc creator, DestroyFunc deleter, PowerFunc power,
		RequirementsFunc requirements);

	virtual OrganizationMod* CreateOrganization(const std::string& id);

	// 转发调用注册时提供的static函数，不需要任何OrganizationMod实例存在。id未注册时
	// 返回0.f。
	virtual float GetPower(const std::string& id) const;

	// 转发注册时提供的static函数，同样不需要实例。id未注册或未启用时返回空表。
	virtual const std::unordered_map<std::string, std::pair<int, int>>& GetRequirements(const std::string& id) const;

	// 必须走注册时mod提供的deleter释放,不能Factory直接delete——跨DLL new/delete安全,
	// 通过liveInstances反查实例对应的注册id、再取出对应deleter调用。
	virtual void DestroyOrganization(OrganizationMod* instance);

	virtual bool CheckRegistered(const std::string& id) const;
	virtual std::vector<std::string> GetRegisteredIds() const;

	// 当前存活的实例和它们的注册id——存档收集各mod的命名计数器/状态版本用。
	virtual std::vector<std::pair<std::string, OrganizationMod*>> GetLiveInstances() const;

	// 设置这个concept从config.json"organization_mods"数组解析出的(id, 参数字符串)表,
	// 必须在调用方触发mod的RegisterModOrganizations导出函数之前调用,否则CreateOrganization
	// 拿实例创建时查不到对应参数。
	virtual void SetModArgs(const std::unordered_map<std::string, std::string>& argsById);

private:
	// 启用=已注册且id在configuredArgs里,详见配对md。
	bool IsEnabled(const std::string& id) const;

	struct Entry {
		CreateFunc creator;
		DestroyFunc deleter;
		PowerFunc power;
		RequirementsFunc requirements;
	};

	std::unordered_map<std::string, Entry> registries;
	std::unordered_map<OrganizationMod*, std::string> liveInstances;
	std::unordered_map<std::string, std::string> configuredArgs;
};
