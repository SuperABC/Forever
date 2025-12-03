#pragma once

#include "asset_base.h"


// 子类注册函数
typedef void (*RegisterModAssetsFunc)(AssetFactory* factory);

// 主程序检测子类
class TestAsset : public Asset {
public:
    TestAsset();
    ~TestAsset();

    static std::string GetId() { return "test"; }
    virtual std::string GetType() const override { return "test"; }
    virtual std::string GetName() const override { return "测试资产"; }

private:

};
