#include "terrain_base.h"


using namespace std;

void TerrainFactory::RegisterTerrain(const string& id, function<unique_ptr<Terrain>()> creator) {
    registries[id] = creator;
    configs[id] = false;
}

unique_ptr<Terrain> TerrainFactory::CreateTerrain(const string& id) const {
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool TerrainFactory::CheckRegistered(const string& id) const {
    return registries.find(id) != registries.end();
}

void TerrainFactory::SetConfig(std::string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}

vector<unique_ptr<Terrain>> TerrainFactory::GetTerrains() const {
    vector<unique_ptr<Terrain>> terrains;
    for (auto r : registries) {
        terrains.push_back(r.second());
    }
    return terrains;
}

