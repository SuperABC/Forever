#pragma once

#include "traffic/station_mod.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>


class StationFactory {
public:
	using CreateFunc = StationMod*(*)(const std::string&);
	using DestroyFunc = void(*)(StationMod*);
	using RoadAssignFunc = void(*)(const std::vector<Road*>&, RoadStationEmitFunc, void*);

	StationFactory() = default;
	virtual ~StationFactory() = default;

	virtual void RegisterStation(const std::string& id, CreateFunc creator, DestroyFunc deleter,
		RoadAssignFunc assign);

	virtual StationMod* CreateStation(const std::string& id);

	// 必须走注册时mod提供的deleter释放,不能Factory直接delete——跨DLL new/delete安全,
	// 通过liveInstances反查实例对应的注册id、再取出对应deleter调用。
	virtual void DestroyStation(StationMod* instance);

	virtual bool CheckRegistered(const std::string& id) const;
	virtual std::vector<std::string> GetRegisteredIds() const;

	// 当前存活的实例和它们的注册id——存档收集各mod的命名计数器/状态版本用。
	virtual std::vector<std::pair<std::string, StationMod*>> GetLiveInstances() const;

	// 设置这个concept从config.json"station_mods"数组解析出的(id, 参数字符串)表,
	// 必须在调用方触发mod的RegisterModStations导出函数之前调用,否则CreateStation
	// 拿实例创建时查不到对应参数。
	virtual void SetModArgs(const std::unordered_map<std::string, std::string>& argsById);

	// 转发调用注册时提供的static函数，不需要任何StationMod实例存在——和BuildingFactory::Assign
	// 同一个模式(见building_factory.h)。id未注册/未启用时直接不调用emit。
	virtual void AssignRoads(const std::string& id, const std::vector<Road*>& roads,
		RoadStationEmitFunc emit, void* context) const;

private:
	// 启用=已注册且id在configuredArgs里,详见配对md。
	bool IsEnabled(const std::string& id) const;

	struct Entry {
		CreateFunc creator;
		DestroyFunc deleter;
		RoadAssignFunc assign;
	};

	std::unordered_map<std::string, Entry> registries;
	std::unordered_map<StationMod*, std::string> liveInstances;
	std::unordered_map<std::string, std::string> configuredArgs;
};
