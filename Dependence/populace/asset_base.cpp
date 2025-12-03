#include "asset_base.h"


using namespace std;

void AssetFactory::RegisterAsset(const string& id, function<unique_ptr<Asset>()> creator) {
    registries[id] = creator;
    configs[id] = false;
}

unique_ptr<Asset> AssetFactory::CreateAsset(const string& id) {
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool AssetFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void AssetFactory::SetConfig(string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}
