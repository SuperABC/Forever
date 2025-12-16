#pragma once

#include "terrain_base.h"


// 子类注册函数
typedef void (*RegisterModTerrainsFunc)(TerrainFactory* factory);

// 海洋地形
class OceanTerrain : public Terrain {
public:
    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

    virtual float GetPriority() const override;

    virtual void DistributeTerrain(int width, int height,
        std::function<bool(int, int, const std::string)> set, std::function<std::string(int, int)> get) const override;
};

// 山区地形
class MountainTerrain : public Terrain {
public:
    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

    virtual float GetPriority() const override;

    virtual void DistributeTerrain(int width, int height,
        std::function<bool(int, int, const std::string)> set, std::function<std::string(int, int)> get) const override;
};



