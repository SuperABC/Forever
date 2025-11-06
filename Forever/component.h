#pragma once

#include "component_base.h"


// 子类注册函数
typedef void (*RegisterModComponentsFunc)(ComponentFactory* factory);

// 主程序检测子类
class TestComponent : public Component {
public:
    virtual std::string GetName() const override { return "测试组合"; }
};
