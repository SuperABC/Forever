#include "roadnet_base.h"


using namespace std;

void RoadnetFactory::RegisterRoadnet(const string& id, function<unique_ptr<Roadnet>()> creator) {
    registry[id] = creator;
}

unique_ptr<Roadnet> RoadnetFactory::CreateRoadnet(const string& id) {
    auto it = registry.find(id);
    if (it != registry.end()) {
        return it->second();
    }
    return nullptr;
}

bool RoadnetFactory::CheckRegistered(const string& id) {
    return registry.find(id) != registry.end();
}

