#pragma once

#include "terrain_base.h"


// 子类注册函数
typedef void (*RegisterModTerrainsFunc)(TerrainFactory* factory);

// 主程序检测子类
class TestTerrain : public Terrain {
public:
    virtual std::string GetName() const override { return "测试地形"; }
};
