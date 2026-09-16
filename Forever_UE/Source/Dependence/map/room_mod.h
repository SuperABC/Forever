#pragma once

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

	// config.json里"room_mods"数组中该mod id后面的命令行式参数字符串(如
	// "pengzhan --density 1.0"里的"--density 1.0"),由<Concept>Factory::Create<Concept>
	// 创建实例后立刻调用一次。默认空实现,不需要参数的mod不用重写,格式解析完全由重写者
	// 自己决定。
	virtual void ApplyArgs(const std::string& args) {}

	// 4类占位能力声明，照抄老工程RoomMod同名字段(E:\Projects\Forever_UE\Source\Dependence\
	// map\room_mod.h)——这次进入populace域时第一次真正加进来。只有"住宅"这次有真实数据
	// (ResidenceRoom在自己构造函数里设isResidential=true+residentialCapacity=1，见
	// Source/Basic/map/room_residence.cpp)，另外3类先占位成false/空值，等对应域
	// (Job/Industry)迁移时再由各自的具体RoomMod子类真正启用，这次不接任何生成/消费逻辑。
	bool isResidential = false;
	int residentialCapacity = 0;      // 床位数

	bool isWorkspace = false;
	int workspaceCapacity = 0;        // 工位数

	bool isStorage = false;
	std::unordered_map<std::string, float> storageConfig; // 仓库属性(分区名->容量)

	bool isManufacture = false;
	std::vector<std::string> manufactureTypes;             // 产线描述(类型列表)
};
