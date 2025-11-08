#include "zone.h"


using namespace std;

std::function<void(ZoneFactory*, const std::vector<std::shared_ptr<Plot>>&)> TestZone::ZoneGenerator =
    [](ZoneFactory* factory, const std::vector<std::shared_ptr<Plot>>& plots) {
        for (const auto& plot : plots) {
            auto zone = factory->CreateZone(TestZone::GetId());
            if (zone) {
                std::string name = zone->GetName();
                plot->AddZone(name, std::move(zone));
            }
        }
    };