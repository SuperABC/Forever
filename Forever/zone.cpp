#include "zone.h"


using namespace std;

int FlatZone::count = 0;

FlatZone::FlatZone() {
    name = count++;
}

std::string FlatZone::GetId() {
    return "flat";
}

std::string FlatZone::GetType() const {
    return "flat";
}

std::string FlatZone::GetName() const {
    return "公寓园区" + std::to_string(name);
}

function<void(ZoneFactory*, BuildingFactory*, const vector<shared_ptr<Plot>>&)> FlatZone::ZoneGenerator =
    [](ZoneFactory* zoneFactory, BuildingFactory* buildingFactory, const vector<shared_ptr<Plot>>& plots) {
        for (const auto& plot : plots) {
            auto zone = zoneFactory->CreateZone(FlatZone::GetId());
            if (zone) {
                zone->SetAcreage(40000.f);

                vector<pair<string, float>> buildings;
                int num = 40;
                for (int i = 0; i < num; i++) {
					buildings.push_back({ "flat", 1.f });
                }
				zone->AddBuildings(buildingFactory, buildings);

                string name = zone->GetName();
                plot->AddZone(name, move(zone));
            }
        }
    };