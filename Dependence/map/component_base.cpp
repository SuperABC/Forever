#include "component_base.h"


using namespace std;

void ComponentFactory::RegisterComponent(const string& id, function<unique_ptr<Component>()> creator) {
    registry[id] = creator;
}

unique_ptr<Component> ComponentFactory::CreateComponent(const string& id) {
    auto it = registry.find(id);
    if (it != registry.end()) {
        return it->second();
    }
    return nullptr;
}

bool ComponentFactory::CheckRegistered(const string& id) {
    return registry.find(id) != registry.end();
}

