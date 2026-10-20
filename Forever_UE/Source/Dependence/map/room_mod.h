#pragma once

#include "common/mod_state.h"
#include "map/feature_spec.h"
#include "map/general_geometry.h"

#include <string>
#include <unordered_map>
#include <vector>


// RoomMod：一个房间类型的标签(比如"residential"/"office")，不是像Zone/Building那样要
// 参与"在地图上竞争地块"的顶层concept——Room永远是Building自己在Layout()里通过
// AssignRoom/ArrangeRow显式创建的，不需要Assign/RandomAcreage/GetPower这套static注册
// 机制，接口保持GetType()/GetName()两个虚方法即可(和阶段3骨架一样极简，只是这次真正有
// 调用方了，见Source/Core/map/room.h/building.md)。
class RoomMod {
public:
	RoomMod() = default;
	virtual ~RoomMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 具体子类必须在SetProperty()里填下面这几组字段，不要在构造函数里赋值——构造函数只
	// 负责id/count这类登记，和BuildingMod/ZoneMod::Layout()、StorageMod::SetProperty()
	// 同一套"两段式"约定。Core创建完mod实例后会立刻调一次这个方法，再读这些字段。
	virtual void SetProperty() {}

	// 4类占位能力声明，照抄老工程RoomMod同名字段(E:\Projects\Forever_UE\Source\Dependence\
	// map\room_mod.h)——这次进入populace域时第一次真正加进来。只有"住宅"这次有真实数据
	// (ResidenceRoom的SetProperty()里设isResidential=true+residentialCapacity=1，见
	// Source/Basic/map/room_basic.cpp)，另外3类先占位成false/空值，等对应域
	// (Job/Industry)迁移时再由各自的具体RoomMod子类真正启用，这次不接任何生成/消费逻辑。
	bool isResidential = false;
	int residentialCapacity = 0;      // 床位数

	bool isWorkspace = false;
	int workspaceCapacity = 0;        // 工位数

	bool isStorage = false;
	std::unordered_map<std::string, float> storageConfig; // 仓库属性(分区名->容量)

	bool isManufacture = false;
	std::vector<std::string> manufactureTypes;             // 产线描述(类型列表)

	// 停车位——老工程isParking/parkingSpaces这次补上，并且比老工程多了旋转(见general_geometry.h
	// ParkingSpot的说明，老工程只有位置，旋转从来没被真正用过)。Traffic::Init()遍历所有
	// isParking的Room，对每个ParkingSpot生成一辆预置车辆，见Source/Core/traffic/traffic.md。
	bool isParking = false;
	std::vector<ParkingSpot> parkingSpots;

	// 这个房间(Room)挂的Script：scriptModName是ScriptMod的id，milestoneNames是要读取的里程碑脚本名
	// 数组(Config::GetScriptPath解析)，和DoorSpec/ElevatorCabinSpec同一个约定——milestoneNames
	// 非空才创建Script，空=没有脚本(默认)。玩家进出这个房间(Room)的enter/leave事件只发给它自己的
	// Script，不再广播给全部脚本，见Core/map/map.md"实体脚本"一节。在SetProperty()里设置。
	std::string scriptModName = "script_empty";
	std::vector<std::string> milestoneNames;

	// 房间门——负责single/row实例化出的Room的门，tag->DoorSpec，在SetProperty()里填写
	// (和这个class其余字段同一个"两段式"约定，不在构造函数里填)。同一个RoomMod类型的所有
	// 房间共用这张表。查找规则和建筑门一致，见feature_spec.h/door.h。
	std::unordered_map<std::string, DoorSpec> doorSpecs;

	// ---- 存档(见Source/Dependence/common/mod_state.md) ----
	// mod私有数据的存档/读档钩子，默认不存任何东西。基类里的公共声明字段由Core直接快照，
	// 这里只放mod自己的私有数据。
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	// SaveState写出数据的格式版本：改了自己SaveState的格式就加一，读档时从
	// StateReader::GetSavedVersion()拿到当时的版本自己迁移。默认0。
	virtual int GetStateVersion() const { return 0; }
	// 这个mod类用来生成唯一实例名的静态计数器——存档保存、读档后恢复，避免读档后新建的实例和
	// 恢复出来的名字撞车。没有计数器的类保持默认(返回-1、Set不做事)。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
