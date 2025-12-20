#pragma once

#include "zone_base.h"


// 子类注册函数
typedef void (*RegisterModZonesFunc)(ZoneFactory* factory);

// 公寓园区
class FlatZone : public Zone {
public:
    FlatZone();

    static std::string GetId();
    virtual std::string GetType() const;
    virtual std::string GetName() const;

    static std::function<void(ZoneFactory*, BuildingFactory*, const std::vector<std::shared_ptr<Plot>>&)> ZoneGenerator;
    
private:
    static int count;

    int name;
};

