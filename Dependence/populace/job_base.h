#pragma once

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
    virtual std::string GetName() const = 0;

	// 父类实现方法

protected:
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