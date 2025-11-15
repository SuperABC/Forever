#include "zone.h"


using namespace std;

std::function<void(ZoneFactory*, BuildingFactory*, const std::vector<std::shared_ptr<Plot>>&)> TestZone::ZoneGenerator =
    [](ZoneFactory* zoneFactory, BuildingFactory* buildingFactory, const std::vector<std::shared_ptr<Plot>>& plots) {
        for (const auto& plot : plots) {
            auto zone = zoneFactory->CreateZone(TestZone::GetId());
            if (zone) {
                zone->SetAcreage(40000.f);
				zone->AddBuildings(buildingFactory, { {"test", 1.f}, {"test", 1.f} });
                std::string name = zone->GetName();
                plot->AddZone(name, std::move(zone));
            }
        }
    };