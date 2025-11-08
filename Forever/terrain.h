#pragma once

#include "terrain_base.h"


// 子类注册函数
typedef void (*RegisterModTerrainsFunc)(TerrainFactory* factory);

// 主程序检测子类
class TestTerrain : public Terrain {
public:
    static std::string GetId() { return "test"; }
    virtual std::string GetName() const override { return "测试地形"; }

    virtual float GetPriority() const override { return 1.0f; };

    virtual void DistributeTerrain(int width, int height,
        std::function<bool(int, int, const std::string)> set, std::function<std::string(int, int)> get) const override {
        for (int i = 0; i < width; i++) {
            for (int j = 0; j < 32; j++) {
                set(i, j, GetName());
                set(i, height - j - 1, GetName());
            }
        }
        for (int j = 0; j < height; j++) {
            for (int i = 0; i < 32; i++) {
                set(i, j, GetName());
                set(width - i - 1, j, GetName());
            }
        }
    }
};
