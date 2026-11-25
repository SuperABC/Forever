#pragma once

#include "polit/party_mod.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>


class PartyFactory {
public:
	using CreateFunc = PartyMod*(*)(const std::string&);
	using DestroyFunc = void(*)(PartyMod*);

	PartyFactory() = default;
	virtual ~PartyFactory() = default;

	virtual void RegisterParty(const std::string& id, CreateFunc creator, DestroyFunc deleter);

	virtual PartyMod* CreateParty(const std::string& id);

	// 必须走注册时mod提供的deleter释放,不能Factory直接delete——跨DLL new/delete安全,
	// 通过liveInstances反查实例对应的注册id、再取出对应deleter调用。
	virtual void DestroyParty(PartyMod* instance);

	virtual bool CheckRegistered(const std::string& id) const;
	// 已启用的id，按字典序排序(党派创建顺序要确定，成员抽样才可复现)。
	virtual std::vector<std::string> GetRegisteredIds() const;

	// 当前存活的实例和它们的注册id——存档收集各mod的命名计数器/状态版本用。
	virtual std::vector<std::pair<std::string, PartyMod*>> GetLiveInstances() const;

	// 设置这个concept从config.json"party_mods"数组解析出的(id, 参数字符串)表,
	// 必须在调用方触发mod的RegisterModParties导出函数之前调用,否则CreateParty
	// 拿实例创建时查不到对应参数。
	virtual void SetModArgs(const std::unordered_map<std::string, std::string>& argsById);

	// 注销不在keepIds里的注册项，返回注销了几个。注册项里的函数指针指向mod dll的代码，dll卸载前必须先注销；
	// 还有存活实例的id要留在keepIds里(销毁它要用注册时的deleter)。见Core/common/registry.md"卸载"。
	virtual int UnregisterExcept(const std::unordered_set<std::string>& keepIds);

private:
	// 启用=已注册且id在configuredArgs里。
	bool IsEnabled(const std::string& id) const;

	struct Entry {
		CreateFunc creator;
		DestroyFunc deleter;
	};

	std::unordered_map<std::string, Entry> registries;
	std::unordered_map<PartyMod*, std::string> liveInstances;
	std::unordered_map<std::string, std::string> configuredArgs;
};
