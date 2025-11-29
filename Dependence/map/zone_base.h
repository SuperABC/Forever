#pragma once

#include "../common/rect.h"
#include "../common/plot.h"

#include "building_base.h"

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>


class Rect;
class Plot;
class Building;
class ZoneFactory;
class BuildingFactory;

class Zone : public Rect {
public:
    Zone() = default;
    virtual ~Zone() = default;

	// 子类实现方法

    // 动态返回园区静态信息
    static std::string GetId();
    virtual std::string GetType() const = 0;
    virtual std::string GetName() const = 0;
    
    // 在地图中生成园区
    static std::function<void(ZoneFactory*, const std::vector<std::shared_ptr<Plot>>&)> ZoneGenerator;

	// 父类实现方法

    // 关联地块
    void SetParent(std::shared_ptr<Plot> plot);
    std::shared_ptr<Plot> GetParent() const;

    // 获取/添加园区内建筑
    std::shared_ptr<Building> GetBuilding(std::string name);
    std::unordered_map<std::string, std::shared_ptr<Building>>& GetBuildings();
    void AddBuildings(BuildingFactory* factory, std::vector<std::pair<std::string, float>> list);

    // 自动分布建筑
    void ArrangeBuildings();

protected:
    std::shared_ptr<Plot> parentPlot;

    std::unordered_map<std::string, std::shared_ptr<Building>> buildings;
};

class ZoneFactory {
public:
    using GeneratorFunc = std::function<void(
        ZoneFactory *, BuildingFactory*, const std::vector<std::shared_ptr<Plot>>&)>;

    void RegisterZone(const std::string& id,
        std::function<std::unique_ptr<Zone>()> creator, GeneratorFunc generator = nullptr);
    std::unique_ptr<Zone> CreateZone(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);
    void GenerateAll(const std::vector<std::shared_ptr<Plot>>& plots, BuildingFactory* factory);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Zone>()>> registries;
    std::unordered_map<std::string, bool> configs;
    std::unordered_map<std::string, GeneratorFunc> generators;
};
