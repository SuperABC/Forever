#pragma once

#include "component_base.h"


// 子类注册函数
typedef void (*RegisterModComponentsFunc)(ComponentFactory* factory);

// 公寓组合
class FlatComponent : public Component {
public:
    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;
};

// 酒店组合
class HotelComponent : public Component {
public:
    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;
};
