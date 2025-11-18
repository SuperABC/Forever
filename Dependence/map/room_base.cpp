#include "room_base.h"


using namespace std;

void Room::SetParent(shared_ptr<Building> building) {
    parentBuilding = building;
}

void Room::SetParent(shared_ptr<Component> component) {
    parentComponent = component;
}

shared_ptr<Building> Room::GetParentBuilding() const {
    return parentBuilding;
}

shared_ptr<Component> Room::GetParentComponent() const {
    return parentComponent;
}

int Room::GetLayer() const {
    return layer;
}

void Room::SetLayer(int layer) {
    this->layer = layer;
}

int Room::GetFace() const {
    return face;
}

void Room::SetFace(int face) {
    this->face = face;
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

void RoomFactory::SetConfig(string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}

