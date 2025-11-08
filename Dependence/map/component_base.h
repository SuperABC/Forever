#pragma once

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Component {
public:
    Component() = default;
    virtual ~Component() = default;

    // 动态返回组合名称
    virtual std::string GetName() const = 0;

protected:
};

class ComponentFactory {
public:
    void RegisterComponent(const std::string& id, std::function<std::unique_ptr<Component>()> creator);
    std::unique_ptr<Component> CreateComponent(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Component>()>> registries;
    std::unordered_map<std::string, bool> configs;
};