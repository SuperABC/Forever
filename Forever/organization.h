#pragma once

#include "../map/component_base.h"
#include "../populace/job_base.h"
#include "organization_base.h"


// 子类注册函数
typedef void (*RegisterModOrganizationsFunc)(OrganizationFactory* factory);

// 默认组织
class DefaultOrganization : public Organization {
public:
    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

    static float GetPower();

    virtual std::vector<std::pair<std::string, std::pair<int, int>>> ComponentRequirements() const override;

    virtual std::vector<std::pair<std::string, std::vector<std::vector<std::string>>>> ArrageVacancies(
        std::vector<std::pair<std::string, int>> components) const override;

    virtual void SetCalendar(CalendarFactory* factory) override;

    virtual void ArrangeRooms() override;
};


