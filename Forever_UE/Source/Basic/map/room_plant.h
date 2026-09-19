#pragma once

#include "map/room_mod.h"

// FactoryRoom：照抄老工程(E:\Projects\Forever_UE\Source\Basic\map\room_basic.h/.cpp的
// FactoryRoom::ConfigRoom) isManufacture=true, manufactureTypes={"experience"}——
// "experience"这个生产类型对应的ExperienceManufacture/ExperienceProduct属于Industry域，
// 这次不迁移，manufactureTypes这个占位字段先带上真实字符串，真正被消费留到Industry域
// 迁移时再接，见building_factory.md。
//
// 文件名是room_plant.h而不是room_factory.h：Source/Dependence/map/room_factory.h已经是
// RoomFactory注册表类的头文件，同一个相对路径"map/room_factory.h"不能在Basic和Dependence
// 两个include目录下各放一份同名文件，否则basic.cpp里#include "map/room_factory.h"这一行
// (本意是引用RoomFactory注册表)会有被这个新文件意外遮蔽的风险。类名/GetId()仍然是
// FactoryRoom/"room_factory"，只有文件名避开了冲突。
class FactoryRoom : public RoomMod {
public:
	FactoryRoom();

	static const char* GetId() { return "room_factory"; }
	virtual const char* GetType() const override { return "room_factory"; }
	virtual const char* GetName() override { return "FactoryRoom"; }
};
