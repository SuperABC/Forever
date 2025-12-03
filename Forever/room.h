#pragma once

#include "room_base.h"


// 子类注册函数
typedef void (*RegisterModRoomsFunc)(RoomFactory* factory);

// 主程序检测子类
class TestRoom : public Room {
public:
    static std::string GetId() { return "test"; }
    virtual std::string GetType() const override { return "test"; }
    virtual std::string GetName() const override { return "测试房间"; }

    virtual bool IsResidential() const override { return true; };
    virtual bool IsWorkspace() const override { return false; }
    virtual int GetLivingCapacity() const override { return 2; }
    virtual int GetPersonnelCapacity() const override { return 0; }
};
