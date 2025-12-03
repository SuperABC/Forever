#pragma once

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Asset {
public:
    Asset() = default;
    virtual ~Asset() = default;

    // 子类实现方法

    // 动态返回姓名静态信息
    static std::string GetId();
    virtual std::string GetType() const = 0;
    virtual std::string GetName() const = 0;

    // 父类实现方法

protected:
};

class AssetFactory {
public:
    void RegisterAsset(const std::string& id, std::function<std::unique_ptr<Asset>()> creator);
    std::unique_ptr<Asset> CreateAsset(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Asset>()>> registries;
    std::unordered_map<std::string, bool> configs;
};