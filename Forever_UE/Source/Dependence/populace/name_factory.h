#pragma once

#include "populace/name_mod.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>


class NameFactory {
public:
	using CreateFunc = NameMod*(*)(const std::string&);
	using DestroyFunc = void(*)(NameMod*);

	NameFactory() = default;
	virtual ~NameFactory() = default;

	virtual void RegisterName(const std::string& id, CreateFunc creator, DestroyFunc deleter);

	virtual NameMod* CreateName(const std::string& id);

	// 必须走注册时mod提供的deleter释放,不能Factory直接delete——跨DLL new/delete安全,
	// 通过liveInstances反查实例对应的注册id、再取出对应deleter调用。
	virtual void DestroyName(NameMod* instance);

	virtual bool CheckRegistered(const std::string& id) const;
	virtual std::vector<std::string> GetRegisteredIds() const;

	// 当前存活的实例和它们的注册id——存档收集各mod的命名计数器/状态版本用。
	virtual std::vector<std::pair<std::string, NameMod*>> GetLiveInstances() const;

	// 设置这个concept从config.json"name_mods"数组解析出的(id, 参数字符串)表,
	// 必须在调用方触发mod的RegisterModNames导出函数之前调用,否则CreateName
	// 拿实例创建时查不到对应参数。
	virtual void SetModArgs(const std::unordered_map<std::string, std::string>& argsById);

	// 一次只应该有一个取名算法生效——SetConfig标记某个已注册id是否启用(config.json里
	// "name_mods"数组的条目)，GetName返回第一个被标记启用的id(找不到返回空字符串)。
	// 调用方(Populace::InitNames)按这个id唯一决定用哪个mod的取名算法，和
	// Map::InitRoadnet()/RoadnetFactory::GetRoadnet同一个模式。
	virtual void SetConfig(const std::string& id, bool enabled);
	virtual std::string GetName() const;

private:
	struct Entry {
		CreateFunc creator;
		DestroyFunc deleter;
	};

	std::unordered_map<std::string, Entry> registries;
	std::unordered_map<NameMod*, std::string> liveInstances;
	std::unordered_map<std::string, std::string> configuredArgs;
	std::unordered_map<std::string, bool> enabledConfig;
};
