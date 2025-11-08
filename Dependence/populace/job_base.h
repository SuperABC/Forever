#pragma once

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Job {
public:
    Job() = default;
    virtual ~Job() = default;

    virtual std::string GetName() const = 0;

private:
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