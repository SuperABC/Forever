#pragma once

#include "common/mod_state.h"
#include "map/feature_spec.h"
#include "map/general_geometry.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>


// 设计见room_mod.md。
class RoomMod {
public:
	RoomMod() = default;
	virtual ~RoomMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 子类在这里填下面的字段(不在构造函数里);Core创建实例后立刻调一次。
	virtual void SetProperty() {}

	// 能力声明,各域按这些字段消费,见room_mod.md。
	bool isResidence = false;
	int residenceCapacity = 0;      // 床位数

	bool isWorkspace = false;
	int workspaceCapacity = 0;        // 工位数

	bool isStorage = false;
	std::string storageType;          // 对应StorageMod的id，Industry初始化时据此为房间生成Storage

	int64_t rent = 0;                 // 每日房租(分)，所有房间都要交，谁交见Society

	bool isManufacture = false;
	std::vector<std::string> manufactureTypes;             // 产线描述(类型列表)

	// 停车位,Traffic::InitParkedVehicles遍历isParking的Room生成预置车辆,见room_mod.md。
	bool isParking = false;
	std::vector<ParkingSpot> parkingSpots;

	// 房间的Script,milestoneNames非空才创建,见Core/map/map.md"实体脚本"一节。在SetProperty()里设置。
	std::string scriptModName = "script_empty";
	std::vector<std::string> milestoneNames;

	// 房间门,tag->DoorSpec,在SetProperty()里填写,同一RoomMod类型的房间共用,见feature_spec.h/door.h。
	std::unordered_map<std::string, DoorSpec> doorSpecs;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
