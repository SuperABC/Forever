#pragma once

#include "room_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModRoom : public Room {
public:
    std::string GetName() const override { return "模组房间"; }
};

