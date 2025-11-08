#include "zone_base.h"


using namespace std;

void ZoneFactory::RegisterZone(const string& id, function<unique_ptr<Zone>()> creator) {
    registries[id] = creator;
    configs[id] = false;
}

unique_ptr<Zone> ZoneFactory::CreateZone(const string& id) {
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool ZoneFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void ZoneFactory::SetConfig(std::string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}


