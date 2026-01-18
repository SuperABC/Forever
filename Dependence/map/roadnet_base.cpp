#include "roadnet_base.h"

#include <queue>

#undef max


using namespace std;

const vector<Node>& Roadnet::GetNodes() const {
    return nodes;
}

const vector<Connection>& Roadnet::GetConnections() const {
    return connections;
}

const vector<shared_ptr<Plot>>& Roadnet::GetPlots() const {
    return plots;
}

const std::vector<Connection> Roadnet::AutoNavigate(
    std::shared_ptr<Plot> start, std::shared_ptr<Plot> end) const {
    std::vector<Connection> result;

    return result;
}

void RoadnetFactory::RegisterRoadnet(const string& id, function<unique_ptr<Roadnet>()> creator) {
    registries[id] = creator;
}

unique_ptr<Roadnet> RoadnetFactory::CreateRoadnet(const string& id) {
    if(configs.find(id) == configs.end() || !configs.find(id)->second)return nullptr;
    
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
    configs[name] = config;
}

unique_ptr<Roadnet> RoadnetFactory::GetRoadnet() const {
    for (auto config : configs) {
        if (config.second)return registries.find(config.first)->second();
    }
    return nullptr;
}
