#pragma once

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Roadnet {
public:
    Roadnet() = default;
    virtual ~Roadnet() = default;

    // 动态返回路网名称
    virtual std::string GetName() const = 0;

private:
};

class RoadnetFactory {
public:
    void RegisterRoadnet(const std::string& id, std::function<std::unique_ptr<Roadnet>()> creator);
    std::unique_ptr<Roadnet> CreateRoadnet(const std::string& id);
    bool CheckRegistered(const std::string& id);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Roadnet>()>> registry;
};