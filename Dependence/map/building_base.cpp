#include "building_base.h"


using namespace std;

void BuildingFactory::RegisterBuilding(const string& id, function<unique_ptr<Building>()> creator) {
    registry[id] = creator;
}

unique_ptr<Building> BuildingFactory::CreateBuilding(const string& id) {
    auto it = registry.find(id);
    if (it != registry.end()) {
        return it->second();
    }
    return nullptr;
}

bool BuildingFactory::CheckRegistered(const string& id) {
    return registry.find(id) != registry.end();
}

