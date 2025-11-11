#include "room_base.h"


using namespace std;

void Room::SetParent(std::shared_ptr<Building> building) {
    parentBuilding = building;
}

void Room::SetParent(std::shared_ptr<Component> component) {
    parentComponent = component;
}

std::shared_ptr<Building> Room::GetParentBuilding() const {
    return parentBuilding;
}

std::shared_ptr<Component> Room::GetParentComponent() const {
    return parentComponent;
}

void RoomFactory::RegisterRoom(const string& id, function<unique_ptr<Room>()> creator) {
    registries[id] = creator;
    configs[id] = false;
}

unique_ptr<Room> RoomFactory::CreateRoom(const string& id) {
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool RoomFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void RoomFactory::SetConfig(std::string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}

