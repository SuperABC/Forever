#include "terrain_base.h"


using namespace std;

void TerrainFactory::RegisterTerrain(const string& id, function<unique_ptr<Terrain>()> creator) {
    registry[id] = creator;
}

unique_ptr<Terrain> TerrainFactory::CreateTerrain(const string& id) const {
    auto it = registry.find(id);
    if (it != registry.end()) {
        return it->second();
    }
    return nullptr;
}

bool TerrainFactory::CheckRegistered(const string& id) const {
    return registry.find(id) != registry.end();
}

vector<unique_ptr<Terrain>> TerrainFactory::GetTerrains() const {
    vector<unique_ptr<Terrain>> terrains;
    for (auto r : registry) {
        terrains.push_back(r.second());
    }
    return terrains;
}

