#include "building_base.h"


using namespace std;

void Building::SetParent(std::shared_ptr<Plot> plot) {
    parentPlot = plot;
}

void Building::SetParent(std::shared_ptr<Zone> zone) {
    parentZone = zone;
}

std::shared_ptr<Plot> Building::GetParentPlot() const {
    return parentPlot;
}

std::shared_ptr<Zone> Building::GetParentZone() const {
    return parentZone;
}

void Building::ReadFloor(int layer, std::string path) {

}

void Building::ReadFloors(std::string path) {

}

void Building::ReadFloors(std::vector<std::string> path) {

}

void Building::AssignRoom(int floor, int slot, std::string room) {

}

void Building::ArrangeRow(int floor, int slot, std::string room) {

}


void BuildingFactory::RegisterBuilding(const string& id, function<unique_ptr<Building>()> creator, vector<float> power) {
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

const unordered_map<string, vector<float>>& BuildingFactory::GetPowers() const {
    return powers;
}

