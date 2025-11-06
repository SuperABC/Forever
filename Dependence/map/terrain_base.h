#pragma once

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Terrain {
public:
    Terrain() = default;
    virtual ~Terrain() = default;

    // 动态返回地形名称
    virtual std::string GetName() const = 0;

private:
};

class TerrainFactory {
public:
    void RegisterTerrain(const std::string& id, std::function<std::unique_ptr<Terrain>()> creator);
    std::unique_ptr<Terrain> CreateTerrain(const std::string& id);
    bool CheckRegistered(const std::string& id);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Terrain>()>> registry;
};