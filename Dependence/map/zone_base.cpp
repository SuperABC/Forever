#include "zone_base.h"


using namespace std;

void Zone::SetParent(std::shared_ptr<Plot> plot) {
    parentPlot = plot;
}

std::shared_ptr<Plot> Zone::GetParent() const {
    return parentPlot;
}

void ZoneFactory::RegisterZone(const string& id,
    function<unique_ptr<Zone>()> creator,  GeneratorFunc generator) {
    registries[id] = creator;
    configs[id] = false;
    generators[id] = generator;
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

void ZoneFactory::GenerateAll(const std::vector<std::shared_ptr<Plot>>& plots) {
    for (const auto& [id, generator] : generators) {
        if (generator && configs[id]) {
            generator(this, plots);
        }
    }
}

