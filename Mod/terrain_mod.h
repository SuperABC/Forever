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

    virtual float GetPriority() const override { return 0.0f; };

    virtual void DistributeTerrain(int width, int height,
        std::function<bool(int, int, const std::string)> set, std::function<std::string(int, int)> get) const override {
        for (int i = 0; i < 32; i++) {
            for (int j = 0; j < 32; j++) {
                set(i, j, GetType());
            }
        }
    }
};

