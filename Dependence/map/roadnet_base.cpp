#include "roadnet_base.h"


using namespace std;

const std::vector<Node>& Roadnet::GetNodes() const {
    return nodes;
}

const std::vector<Connection>& Roadnet::GetConnections() const {
    return connections;
}

const std::vector<std::shared_ptr<Plot>>& Roadnet::GetPlots() const {
    return plots;
}

void RoadnetFactory::RegisterRoadnet(const string& id, function<unique_ptr<Roadnet>()> creator) {
    registries[id] = creator;
    configs[id] = false;
}

unique_ptr<Roadnet> RoadnetFactory::CreateRoadnet(const string& id) {
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool RoadnetFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void RoadnetFactory::SetConfig(string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}

unique_ptr<Roadnet> RoadnetFactory::GetRoadnet() const {
    for (auto config : configs) {
        if (config.second)return registries.find(config.first)->second();
    }
    return nullptr;
}
