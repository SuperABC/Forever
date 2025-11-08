#pragma once

#include "../common/plot.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Zone : public Rect {
public:
    Zone() = default;
    virtual ~Zone() = default;

    // 动态返回园区静态信息
    static std::string GetId();
    virtual std::string GetName() const = 0;

protected:
};

class ZoneFactory {
public:
    void RegisterZone(const std::string& id, std::function<std::unique_ptr<Zone>()> creator);
    std::unique_ptr<Zone> CreateZone(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Zone>()>> registries;
    std::unordered_map<std::string, bool> configs;
};