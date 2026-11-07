#pragma once

#include "map/roadnet_mod.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>


class RoadnetFactory {
public:
	using CreateFunc = RoadnetMod*(*)(const std::string&);
	using DestroyFunc = void(*)(RoadnetMod*);

	RoadnetFactory() = default;
	virtual ~RoadnetFactory() = default;

	virtual void RegisterRoadnet(const std::string& id, CreateFunc creator, DestroyFunc deleter);

	virtual RoadnetMod* CreateRoadnet(const std::string& id);

	// 必须走注册时mod提供的deleter释放,不能Factory直接delete——跨DLL new/delete安全,
	// 通过liveInstances反查实例对应的注册id、再取出对应deleter调用。
	virtual void DestroyRoadnet(RoadnetMod* instance);

	virtual bool CheckRegistered(const std::string& id) const;
	virtual std::vector<std::string> GetRegisteredIds() const;

	// 当前存活的实例和它们的注册id——存档收集各mod的命名计数器/状态版本用。
	virtual std::vector<std::pair<std::string, RoadnetMod*>> GetLiveInstances() const;

	// 设置这个concept从config.json"roadnet_mods"数组解析出的(id, 参数字符串)表,
	// 必须在调用方触发mod的RegisterModRoadnets导出函数之前调用,否则CreateRoadnet
	// 拿实例创建时查不到对应参数。
	virtual void SetModArgs(const std::unordered_map<std::string, std::string>& argsById);

	// 一次只应该有一个路网布局方案生效——SetConfig标记某个已注册id是否启用(config.json里
	// "roadnet_mods"数组的条目),GetRoadnet返回第一个被标记启用的id(找不到返回空字符串)。
	// 调用方(Map::InitRoadnet)按这个id唯一决定加载哪个mod的产出。
	virtual void SetConfig(const std::string& id, bool enabled);
	virtual std::string GetRoadnet() const;

private:
	struct Entry {
		CreateFunc creator;
		DestroyFunc deleter;
	};

	std::unordered_map<std::string, Entry> registries;
	std::unordered_map<RoadnetMod*, std::string> liveInstances;
	std::unordered_map<std::string, std::string> configuredArgs;
	std::unordered_map<std::string, bool> enabledConfig;
};
