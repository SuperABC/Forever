#pragma once

#include "organization_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModOrganization : public Organization {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetType() const override { return "mod"; }
    virtual std::string GetName() const override { return "模组组织"; }

    static float GetPower() {
        return 1.0f;
    }

    virtual std::vector<std::pair<std::string, std::pair<int, int>>> ComponentRequirements() const override {
        return std::vector<std::pair<std::string, std::pair<int, int>>>();
    }

    virtual std::vector<std::pair<std::string, std::vector<std::string>>> ArrageVacancies(
        std::vector<std::pair<std::string, int>> components) const override {
		return std::vector<std::pair<std::string, std::vector<std::string>>>();
    }

    virtual void SetCalendar(CalendarFactory* factory) override {
        for (auto component : jobs) {
            for (auto job : component.second) {
                std::shared_ptr<Calendar> calendar = factory->CreateCalendar("mod");
                job.first->SetCalendar(calendar);
            }
        }
    }

};

