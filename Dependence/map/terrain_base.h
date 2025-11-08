#pragma once

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Terrain {
public:
    Terrain() = default;
    virtual ~Terrain() = default;

    // 动态返回地形静态信息
    static std::string GetId();
    virtual std::string GetName() const = 0;

    // 动态返回生成优先级
    virtual float GetPriority() const = 0;

    // 在地图上生成地形
    virtual void DistributeTerrain(int width, int height,
        std::function<bool(int, int, const std::string)> set, std::function<std::string(int, int)> get) const = 0;

protected:
};

class TerrainFactory {
public:
    void RegisterTerrain(const std::string& id, std::function<std::unique_ptr<Terrain>()> creator);
    std::unique_ptr<Terrain> CreateTerrain(const std::string& id) const;
    bool CheckRegistered(const std::string& id) const;
    void SetConfig(std::string name, bool config);
    std::vector<std::unique_ptr<Terrain>> GetTerrains() const;

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Terrain>()>> registries;
    std::unordered_map<std::string, bool> configs;
};