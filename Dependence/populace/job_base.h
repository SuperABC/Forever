#pragma once

#include "../common/utility.h"
#include "../society/calendar_base.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Job {
public:
    Job() = default;
    virtual ~Job() = default;

	// 子类实现方法

    // 动态返回职业静态信息
    static std::string GetId();
    virtual std::string GetType() const = 0;
    virtual std::string GetName() const = 0;

    // 职业剧本
    virtual std::vector<std::string> GetScripts() const = 0;

	// 父类实现方法

	// 设置/获取日历
    void SetCalendar(std::shared_ptr<Calendar> &calendar);
    std::shared_ptr<Calendar> &GetCalendar();

protected:
    std::shared_ptr<Calendar> calendar;

};

class JobFactory {
public:
    void RegisterJob(const std::string& id, std::function<std::unique_ptr<Job>()> creator);
    std::unique_ptr<Job> CreateJob(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Job>()>> registries;
    std::unordered_map<std::string, bool> configs;
};