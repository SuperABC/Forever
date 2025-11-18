#pragma once

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Name {
public:
    Name() = default;
    virtual ~Name() = default;

    // 子类实现方法

    // 动态返回姓名静态信息
    static std::string GetId();
    virtual std::string GetName() const = 0;

    // 生成随机姓名
    virtual std::string GenerateName(bool male = true, bool female = true, bool neutral = true) = 0;

    // 父类实现方法

protected:
};

class NameFactory {
public:
    void RegisterName(const std::string& id, std::function<std::unique_ptr<Name>()> creator);
    std::unique_ptr<Name> CreateName(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);
    std::unique_ptr<Name> GetName() const;

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Name>()>> registries;
    std::unordered_map<std::string, bool> configs;
};