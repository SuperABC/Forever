#pragma once

#include "traffic/vehicle_mod.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>


class VehicleFactory {
public:
	using CreateFunc = VehicleMod*(*)(const std::string&);
	using DestroyFunc = void(*)(VehicleMod*);
	// 载具种类("car"/"bus"/"plane"...)：每个具体VehicleMod子类的static const char* GetCategory()，
	// 不需要实例——和OrganizationFactory::PowerFunc同一个模式。返回字面量指针而不是std::string，
	// 避免跨DLL传std::string(SSO/分配器问题)；工厂在宿主侧(下面的virtual方法内)把它拷成自己的string。
	using CategoryFunc = const char*(*)();

	VehicleFactory() = default;
	virtual ~VehicleFactory() = default;

	virtual void RegisterVehicle(const std::string& id, CreateFunc creator, DestroyFunc deleter, CategoryFunc category);

	virtual VehicleMod* CreateVehicle(const std::string& id);

	// 必须走注册时mod提供的deleter释放,不能Factory直接delete——跨DLL new/delete安全,
	// 通过liveInstances反查实例对应的注册id、再取出对应deleter调用。
	virtual void DestroyVehicle(VehicleMod* instance);

	virtual bool CheckRegistered(const std::string& id) const;
	virtual std::vector<std::string> GetRegisteredIds() const;

	// 某个车型id的载具种类，id未启用返回空串。不需要创建VehicleMod实例。
	virtual std::string GetCategory(const std::string& id) const;

	// 所有已启用且种类==category的车型id——多个VehicleMod可以共用同一个种类名。
	virtual std::vector<std::string> GetIdsByCategory(const std::string& category) const;

	// 当前存活的实例和它们的注册id——存档收集各mod的命名计数器/状态版本用。
	virtual std::vector<std::pair<std::string, VehicleMod*>> GetLiveInstances() const;

	// 设置这个concept从config.json"vehicle_mods"数组解析出的(id, 参数字符串)表,
	// 必须在调用方触发mod的RegisterModVehicles导出函数之前调用,否则CreateVehicle
	// 拿实例创建时查不到对应参数。
	virtual void SetModArgs(const std::unordered_map<std::string, std::string>& argsById);

private:
	// 启用=已注册且id在configuredArgs里,详见配对md。
	bool IsEnabled(const std::string& id) const;

	struct Entry {
		CreateFunc creator;
		DestroyFunc deleter;
		std::string category; // 注册时在宿主侧从CategoryFunc拷出来
	};

	std::unordered_map<std::string, Entry> registries;
	std::unordered_map<VehicleMod*, std::string> liveInstances;
	std::unordered_map<std::string, std::string> configuredArgs;
};
