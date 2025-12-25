#pragma once

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Scheduler {
public:
    Scheduler();
    virtual ~Scheduler();

    // 子类实现方法

    // 动态返回姓名静态信息
    static std::string GetId();
    virtual std::string GetType() const = 0;
    virtual std::string GetName() const = 0;

    // 组织权重
    static float GetPower();

    // 父类实现方法

    // 管理状态
    std::string GetStatus();
    void SetStatus(std::string status);

protected:
    std::string status;
};

class SchedulerFactory {
public:
    void RegisterScheduler(const std::string& id, std::function<std::unique_ptr<Scheduler>()> creator, float power);
    std::unique_ptr<Scheduler> CreateScheduler(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);
    const std::unordered_map<std::string, float>& GetPowers() const;

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Scheduler>()>> registries;
    std::unordered_map<std::string, bool> configs;
    std::unordered_map<std::string, float> powers;
};