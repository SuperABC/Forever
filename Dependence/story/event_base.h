#pragma once

#include "../common/condition.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>

#undef CreateEvent


class Event {
public:
    Event() = default;
    virtual ~Event() = default;

    // 子类实现方法

    // 动态返回组织静态信息
    static std::string GetId();
    virtual std::string GetType() const = 0;
    virtual std::string GetName() const = 0;

    // 判断匹配
    virtual bool operator==(std::shared_ptr<Event> e) = 0;

    // 父类实现方法

    // 设置/获取控制条件
    void SetCondition(Condition condition);
    Condition& GetCondition();

protected:
    Condition condition;
};

class EventFactory {
public:
    void RegisterEvent(const std::string& id, std::function<std::unique_ptr<Event>()> creator);
    std::unique_ptr<Event> CreateEvent(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Event>()>> registries;
    std::unordered_map<std::string, bool> configs;
};