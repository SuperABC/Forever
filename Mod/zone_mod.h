#pragma once

#include "zone_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModZone : public Zone {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetName() const override { return "模组园区"; }

    static std::function<void(ZoneFactory*, BuildingFactory*, const std::vector<std::shared_ptr<Plot>>&)> ZoneGenerator;
};

std::function<void(ZoneFactory*, BuildingFactory*, const std::vector<std::shared_ptr<Plot>>&)> ModZone::ZoneGenerator =
    [](ZoneFactory* zoneFactory, BuildingFactory* buildingFactory, const std::vector<std::shared_ptr<Plot>>& plots) {
        for (const auto& plot : plots) {
            auto zone = zoneFactory->CreateZone(ModZone::GetId());
            if (zone) {
                zone->SetAcreage(40000.f);
                zone->AddBuildings(buildingFactory, { {"mod", 1.f}, {"mod", 1.f} });
                std::string name = zone->GetName();
                plot->AddZone(name, std::move(zone));
            }
        }
    };
