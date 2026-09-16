#pragma once

#include "map/room_mod.h"

// ResidenceRoom：住宅Room——这次进入populace域时第一次给这个类填真内容(之前是阶段3占位
// 骨架，见Source/Core/populace/populace.md"进入populace域"一节)：声明isResidential=true+
// residentialCapacity=1，照抄老工程ResidentialRoom::ConfigRoom的取值(每间住宅room=1个
// 名额，多人合住靠Map::Checkin()的名额池逻辑，不靠这个capacity强制)。workspace/storage/
// manufacture这3类占位属性保持RoomMod基类默认值(false/空)不动——这次只有"住宅"有真实数据，
// 另外3类等Job/Industry域迁移时再由各自的具体RoomMod子类真正启用。
// Basic现在编译为DynamicLibrary(Basic.dll),和Forever_Mod下的Mod一样由Config/ModLoader在
// 运行时扫描加载,不会静态链进Forever.Build.cs,见 Source/Basic/README.md。
class ResidenceRoom : public RoomMod {
public:
	ResidenceRoom();

	static const char* GetId() { return "room_residence"; }
	virtual const char* GetType() const override { return "room_residence"; }
	virtual const char* GetName() override { return "ResidenceRoom"; }
};
