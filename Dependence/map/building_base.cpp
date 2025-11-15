#include "building_base.h"


using namespace std;

Facility::Facility(FACILITY_TYPE type, float x, float y, float w, float h)
    : Rect(x, y, w, h), type(type) {
}


Facility::FACILITY_TYPE Facility::getType() const {
    return type;
}

Floor::Floor(int level, float width, float height)
    : level(level), Rect(0, 0, width, height) {

}

Floor::~Floor() {

}

void Floor::AddFacility(Facility facility) {
    facilities.push_back(facility);
}

void Floor::AddUsage(std::pair<Rect, int> row) {
    rows.push_back(row);
}

void Floor::AddRoom(Rect room) {
    rooms.push_back(room);
}

int Floor::GetLevel() const { return level; }

std::vector<Facility>& Floor::GetFacilities() {
    return facilities;
}

std::vector<std::pair<Rect, int>>& Floor::GetRows() {
    return rows;
}

std::vector<Rect>& Floor::GetRooms() {
    return rooms;
}

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

int Building::GetLayers() const {
    return layers;
}

int Building::GetBasements() const {
    return basements;
}

std::vector<std::shared_ptr<Component>>& Building::GetComponents() {
    return components;
}

std::vector<std::shared_ptr<Room>>& Building::GetRooms() {
    return rooms;
}

std::shared_ptr<Floor> Building::GetFloor(int floor) const {
    if (basements + floor >= 0 && basements + floor < floors.size())
        return floors[basements + floor];
    else return nullptr;
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

