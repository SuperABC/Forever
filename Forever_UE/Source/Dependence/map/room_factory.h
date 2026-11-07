#pragma once

#include "map/room_mod.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>


class RoomFactory {
public:
	using CreateFunc = RoomMod*(*)(const std::string&);
	using DestroyFunc = void(*)(RoomMod*);

	RoomFactory() = default;
	virtual ~RoomFactory() = default;

	virtual void RegisterRoom(const std::string& id, CreateFunc creator, DestroyFunc deleter);

	virtual RoomMod* CreateRoom(const std::string& id);

	// 必须走注册时mod提供的deleter释放,不能Factory直接delete——跨DLL new/delete安全,
	// 通过liveInstances反查实例对应的注册id、再取出对应deleter调用。
	virtual void DestroyRoom(RoomMod* instance);

	virtual bool CheckRegistered(const std::string& id) const;
	virtual std::vector<std::string> GetRegisteredIds() const;

	// 当前存活的实例和它们的注册id——存档收集各mod的命名计数器/状态版本用。
	virtual std::vector<std::pair<std::string, RoomMod*>> GetLiveInstances() const;

	// 设置这个concept从config.json"room_mods"数组解析出的(id, 参数字符串)表,
	// 必须在调用方触发mod的RegisterModRooms导出函数之前调用,否则CreateRoom
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
	std::unordered_map<RoomMod*, std::string> liveInstances;
	std::unordered_map<std::string, std::string> configuredArgs;
};
