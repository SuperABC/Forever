#pragma once

#include <string>

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
};
