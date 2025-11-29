#include "change_base.h"


using namespace std;

void Change::SetCondition(Condition condition) {
    this->condition = condition;
}
Condition& Change::GetCondition() {
    return condition;
}

void ChangeFactory::RegisterChange(const string& id, function<unique_ptr<Change>()> creator) {
    registries[id] = creator;
    configs[id] = false;
}

unique_ptr<Change> ChangeFactory::CreateChange(const string& id) {
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool ChangeFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void ChangeFactory::SetConfig(string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}


