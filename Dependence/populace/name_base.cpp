#include "name_base.h"
#include "job_base.h"
#include "scheduler_base.h"


using namespace std;

void NameFactory::RegisterName(const string& id, function<unique_ptr<Name>()> creator) {
    registries[id] = creator;
}

unique_ptr<Name> NameFactory::CreateName(const string& id) {
    if(configs.find(id) == configs.end() || !configs.find(id)->second)return nullptr;

    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool NameFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void NameFactory::SetConfig(string name, bool config) {
    configs[name] = config;
}

unique_ptr<Name> NameFactory::GetName() const {
    for (auto config : configs) {
        if (config.second)return registries.find(config.first)->second();
    }
    return nullptr;
}
