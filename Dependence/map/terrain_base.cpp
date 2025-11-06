#include "terrain_base.h"


using namespace std;

void TerrainFactory::RegisterTerrain(const string& id, function<unique_ptr<Terrain>()> creator) {
    registry[id] = creator;
}

unique_ptr<Terrain> TerrainFactory::CreateTerrain(const string& id) {
    auto it = registry.find(id);
    if (it != registry.end()) {
        return it->second();
    }
    return nullptr;
}

bool TerrainFactory::CheckRegistered(const string& id) {
    return registry.find(id) != registry.end();
}

