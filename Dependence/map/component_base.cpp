#include "component_base.h"
#include "building_base.h"


using namespace std;

void Component::SetParent(std::shared_ptr<Building> building) {
    parentBuilding = building;
}

std::shared_ptr<Building> Component::GetParent() const {
    return parentBuilding;
}

void ComponentFactory::RegisterComponent(const string& id, function<unique_ptr<Component>()> creator) {
    registries[id] = creator;
    configs[id] = false;
}

unique_ptr<Component> ComponentFactory::CreateComponent(const string& id) {
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool ComponentFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void ComponentFactory::SetConfig(std::string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}

