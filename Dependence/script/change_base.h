#pragma once

#include "../common/condition.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Change {
public:
    Change() = default;
    virtual ~Change() = default;

    // 子类实现方法

    // 动态返回组织静态信息
    static std::string GetId();
    virtual std::string GetName() const = 0;

    // 基变更列表
    virtual std::vector<std::shared_ptr<Change>> ApplyChange() = 0;

    // 父类实现方法

    // 设置/获取控制条件
    void SetCondition(Condition condition);
    Condition& GetCondition();

protected:
    Condition condition;
};

class ChangeFactory {
public:
    void RegisterChange(const std::string& id, std::function<std::unique_ptr<Change>()> creator);
    std::unique_ptr<Change> CreateChange(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Change>()>> registries;
    std::unordered_map<std::string, bool> configs;
};