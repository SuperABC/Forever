#pragma once

#include "room_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModRoom : public Room {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetType() const override { return "mod"; }
    virtual std::string GetName() const override { return "模组房间"; }

    virtual bool IsResidential() const override { return false; };
    virtual bool IsWorkspace() const override { return true; }
    virtual int GetLivingCapacity() const override { return 0; }
    virtual int GetPersonnelCapacity() const override { return 2; }
};

