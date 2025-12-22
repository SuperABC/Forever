#pragma once

#include "../map/component_base.h"
#include "../populace/job_base.h"
#include "organization_base.h"


// 子类注册函数
typedef void (*RegisterModOrganizationsFunc)(OrganizationFactory* factory);

// 主程序检测子类
class TestOrganization : public Organization {
public:
    static std::string GetId() { return "test"; }
    virtual std::string GetType() const override { return "test"; }
    virtual std::string GetName() const override { return "测试组织"; }

    static float GetPower() {
        return 1.0f;
    }

    virtual std::vector<std::pair<std::string, std::pair<int, int>>> ComponentRequirements() const override {
        std::vector<std::pair<std::string, std::pair<int, int>>> requirements;
		requirements.emplace_back("hotel", std::make_pair(1, 1));

		return requirements;
    }

    virtual std::vector<std::pair<std::string, std::vector<std::vector<std::string>>>> ArrageVacancies(
        std::vector<std::pair<std::string, int>> components) const override {
        std::vector<std::pair<std::string, std::vector<std::vector<std::string>>>> jobs;
        for(auto& comp : components) {
            jobs.emplace_back(comp.first, std::vector<std::vector<std::string>>(comp.second));
            for(int i = 0; i < comp.second; i++) {
                jobs.back().second[i] = std::vector<std::string>(10, "hotel_cleaner");
			}
		}

		return jobs;
    }

    virtual void SetCalendar(CalendarFactory* factory) override {
        for (auto component : jobs) {
            for (auto job : component.second) {
                std::shared_ptr<Calendar> calendar = factory->CreateCalendar("test");
                job.first->SetCalendar(calendar);
            }
        }
    }

};


