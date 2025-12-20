#include "component_base.h"
#include "building_base.h"


using namespace std;

void Component::SetParent(shared_ptr<Building> building) {
    parentBuilding = building;
}

shared_ptr<Building> Component::GetParent() const {
    return parentBuilding;
}

vector<shared_ptr<Room>>& Component::GetRooms() {
    return rooms;
}

void Component::AddRoom(shared_ptr<Room> room) {
    rooms.push_back(room);
}

void ComponentFactory::RegisterComponent(const string& id, function<unique_ptr<Component>()> creator) {
    registries[id] = creator;
}

unique_ptr<Component> ComponentFactory::CreateComponent(const string& id) {
    if(configs.find(id) == configs.end() || !configs.find(id)->second)return nullptr;

    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool ComponentFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void ComponentFactory::SetConfig(string name, bool config) {
    configs[name] = config;
}

