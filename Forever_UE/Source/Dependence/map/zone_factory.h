#pragma once

#include "map/zone_mod.h"
#include "map/general_geometry.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>


class ZoneFactory {
public:
	using CreateFunc = ZoneMod*(*)(const std::string&);
	using DestroyFunc = void(*)(ZoneMod*);
	using AssignFunc = void(*)(const std::vector<Lot*>&, PlacementEmitFunc, void*);

	ZoneFactory() = default;
	virtual ~ZoneFactory() = default;

	virtual void RegisterZone(const std::string& id, CreateFunc creator, DestroyFunc deleter,
		AssignFunc assign);

	virtual ZoneMod* CreateZone(const std::string& id);

	// 必须走注册时mod提供的deleter释放,不能Factory直接delete——跨DLL new/delete安全,
	// 通过liveInstances反查实例对应的注册id、再取出对应deleter调用。
	virtual void DestroyZone(ZoneMod* instance);

	virtual bool CheckRegistered(const std::string& id) const;
	virtual std::vector<std::string> GetRegisteredIds() const;

	// 当前存活的实例和它们的注册id——存档收集各mod的命名计数器/状态版本用。
	virtual std::vector<std::pair<std::string, ZoneMod*>> GetLiveInstances() const;

	// 设置这个concept从config.json"zone_mods"数组解析出的(id, 参数字符串)表,
	// 必须在调用方触发mod的RegisterModZones导出函数之前调用,否则CreateZone
	// 拿实例创建时查不到对应参数。
	virtual void SetModArgs(const std::unordered_map<std::string, std::string>& argsById);

	// 转发调用注册时提供的static Assign函数，不需要任何ZoneMod实例存在。id未注册时不调用emit。
	virtual void Assign(const std::string& id, const std::vector<Lot*>& lots,
		PlacementEmitFunc emit, void* context) const;

private:
	// 启用=已注册且id在configuredArgs里,详见配对md。
	bool IsEnabled(const std::string& id) const;

	struct Entry {
		CreateFunc creator;
		DestroyFunc deleter;
		AssignFunc assign;
	};

	std::unordered_map<std::string, Entry> registries;
	std::unordered_map<ZoneMod*, std::string> liveInstances;
	std::unordered_map<std::string, std::string> configuredArgs;
};
