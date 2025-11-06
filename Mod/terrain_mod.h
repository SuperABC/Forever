#pragma once

#include "terrain_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModTerrain : public Terrain {
public:
    std::string GetName() const override { return "模组地形"; }
};

