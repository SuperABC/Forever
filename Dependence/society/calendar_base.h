#pragma once

#include "../common/utility.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Calendar {
public:
    Calendar() = default;
    virtual ~Calendar() = default;

    // 子类实现方法

    // 动态返回组织静态信息
    static std::string GetId();
    virtual std::string GetType() const = 0;
    virtual std::string GetName() const = 0;

    virtual Time SigninTime(Time date) const = 0;
    virtual Time SignoutTime(Time date) const = 0;

    // 父类实现方法

protected:
};

class CalendarFactory {
public:
    void RegisterCalendar(const std::string& id, std::function<std::unique_ptr<Calendar>()> creator);
    std::unique_ptr<Calendar> CreateCalendar(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Calendar>()>> registries;
    std::unordered_map<std::string, bool> configs;
};