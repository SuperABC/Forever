#include "zone.h"


using namespace std;

int DefaultZone::count = 0;

DefaultZone::DefaultZone() {
    name = count++;
}

string DefaultZone::GetId() {
    return "default";
}

string DefaultZone::GetType() const {
    return "default";
}

string DefaultZone::GetName() const {
    return "默认园区" + to_string(name);
}

function<void(ZoneFactory*, BuildingFactory*, const vector<shared_ptr<Plot>>&)> DefaultZone::ZoneGenerator =
    [](ZoneFactory* zoneFactory, BuildingFactory* buildingFactory, const vector<shared_ptr<Plot>>& plots) {
        for (const auto& plot : plots) {
            auto zone = zoneFactory->CreateZone(DefaultZone::GetId());
            if (zone) {
                zone->SetAcreage(40000.f);

                vector<pair<string, float>> buildings;
                int num = 40;
                for (int i = 0; i < num; i++) {
					buildings.push_back({ "default_residential", 1.f });
                    buildings.push_back({ "default_working", 1.f });
                }
				zone->AddBuildings(buildingFactory, buildings);

                string name = zone->GetName();
                plot->AddZone(name, move(zone));
            }
        }
    };