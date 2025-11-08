#pragma once

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Building {
public:
    Building() = default;
    virtual ~Building() = default;

    // 动态返回建筑名称
    virtual std::string GetName() const = 0;

protected:
};

class BuildingFactory {
public:
    void RegisterBuilding(const std::string& id, std::function<std::unique_ptr<Building>()> creator);
    std::unique_ptr<Building> CreateBuilding(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Building>()>> registries;
    std::unordered_map<std::string, bool> configs;
};