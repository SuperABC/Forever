#include "building_base.h"


using namespace std;

void BuildingFactory::RegisterBuilding(const string& id, function<unique_ptr<Building>()> creator, float power) {
    registries[id] = creator;
    configs[id] = false;
    powers[id] = power;
}

unique_ptr<Building> BuildingFactory::CreateBuilding(const string& id) {
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool BuildingFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void BuildingFactory::SetConfig(std::string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}

const unordered_map<string, float>& BuildingFactory::GetPowers() const {
    return powers;
}

