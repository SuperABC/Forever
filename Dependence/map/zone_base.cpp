#include "zone_base.h"


using namespace std;

void ZoneFactory::RegisterZone(const string& id, function<unique_ptr<Zone>()> creator) {
    registry[id] = creator;
}

unique_ptr<Zone> ZoneFactory::CreateZone(const string& id) {
    auto it = registry.find(id);
    if (it != registry.end()) {
        return it->second();
    }
    return nullptr;
}

bool ZoneFactory::CheckRegistered(const string& id) {
    return registry.find(id) != registry.end();
}


