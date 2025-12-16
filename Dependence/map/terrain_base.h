#pragma once

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Terrain {
public:
    Terrain() = default;
    virtual ~Terrain() = default;

	// 子类实现方法

    // 动态返回地形静态信息
    static std::string GetId();
    virtual std::string GetType() const = 0;
    virtual std::string GetName() const = 0;

    // 动态返回生成优先级，高优先级会被低优先级覆盖
    virtual float GetPriority() const = 0;

    // 在地图上生成地形
    virtual void DistributeTerrain(int width, int height,
        std::function<bool(int, int, const std::string)> set, std::function<std::string(int, int)> get) const = 0;

	// 父类实现方法

    // 地形填充，若ovewrite为true，则全图填充，否则只填充平原
    int FloodTerrain(int x, int y, int num, bool overwrite, int width, int height,
        std::function<bool(int, int, const std::string)> set, std::function<std::string(int, int)> get) const;

protected:
    // 地形填充辅助内容
    std::vector<int> dx = { -1, 1, 0, 0 };
    std::vector<int> dy = { 0, 0, -1, 1 };
    bool CheckBoundary(int x, int y, bool overwrite, int width, int height,
        std::function<bool(int, int, const std::string)> set, std::function<std::string(int, int)> get) const;
    void UpdateBoundary(int x, int y, std::vector<std::pair<int, int>>& q, bool overwrite, int width, int height,
        std::function<bool(int, int, const std::string)> set, std::function<std::string(int, int)> get) const;
    void ShapeFilter(int x, int y, int width, int height,
        std::function<bool(int, int, const std::string)> set, std::function<std::string(int, int)> get, int side = 1, float threshold = 0.5f) const;
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