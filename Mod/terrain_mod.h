#pragma once

#include "terrain_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModTerrain : public Terrain {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetType() const override { return "mod"; }
    virtual std::string GetName() const override { return "模组地形"; }

    virtual float GetPriority() const override { return 2.0f; };

    virtual void DistributeTerrain(int width, int height,
        std::function<bool(int, int, const std::string)> set, std::function<std::string(int, int)> get) const override {
        for (int i = 0; i < width; i++) {
            for (int j = 32; j < 64; j++) {
                set(i, j, GetType());
                set(i, height - j - 1, GetType());
            }
        }
        for (int j = 0; j < height; j++) {
            for (int i = 32; i < 64; i++) {
                set(i, j, GetType());
                set(width - i - 1, j, GetType());
            }
        }
    }
};

