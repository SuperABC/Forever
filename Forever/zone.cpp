#include "zone.h"


using namespace std;

int TestZone::count = 0;

function<void(ZoneFactory*, BuildingFactory*, const vector<shared_ptr<Plot>>&)> TestZone::ZoneGenerator =
    [](ZoneFactory* zoneFactory, BuildingFactory* buildingFactory, const vector<shared_ptr<Plot>>& plots) {
        for (const auto& plot : plots) {
            auto zone = zoneFactory->CreateZone(TestZone::GetId());
            if (zone) {
                zone->SetAcreage(40000.f);
				zone->AddBuildings(buildingFactory, { {"test", 1.f}, {"test", 1.f} });
                string name = zone->GetName();
                plot->AddZone(name, move(zone));
            }
        }
    };