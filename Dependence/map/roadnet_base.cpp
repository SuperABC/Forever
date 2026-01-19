#include "roadnet_base.h"
#include "../common/error.h"

#include <queue>
#include <unordered_map>
#include <limits>
#include <algorithm>
#include <functional>

#undef max


using namespace std;

struct pair_hash {
    template <class T1, class T2>
    size_t operator()(const pair<T1, T2>& p) const {
        auto hash1 = hash<T1>{}(p.first);
        auto hash2 = hash<T2>{}(p.second);

        return hash1 ^ (hash2 << 1) ^ (hash2 >> 31);
    }
};

const vector<Node>& Roadnet::GetNodes() const {
    return nodes;
}

const vector<Connection>& Roadnet::GetConnections() const {
    return connections;
}

const vector<shared_ptr<Plot>>& Roadnet::GetPlots() const {
    return plots;
}

const vector<Connection> Roadnet::AutoNavigate(
    shared_ptr<Plot> start, shared_ptr<Plot> end) const {

    if (start == end) return {};

    auto startRoads = start->GetRoads();
    auto endRoads = end->GetRoads();
    if (startRoads.empty() || endRoads.empty()) return {};
    for (auto startRoad : startRoads) {
        for (auto endRoad : endRoads) {
            if (startRoad.first == endRoad.first) {
                if (startRoad.first.GetV1() == endRoad.first.GetV1()) {
                    return { Connection(startRoad.first.GetRoadnet(),
                        startRoad.first.GetV1(), startRoad.first.GetV2(), startRoad.second, endRoad.second) };
                }
                else {
                    return { Connection(startRoad.first.GetRoadnet(),
                        startRoad.first.GetV1(), startRoad.first.GetV2(), startRoad.second, 1.0f - endRoad.second) };
                }
            }
        }
    }

    unordered_map<int, vector<pair<int, float>>> graph;
    unordered_map<pair<int, int>, int, pair_hash> connectionMap;
    unordered_map<int, unordered_map<int, int>> connectionIndexMap;

    // 添加已有道路到图
    for (size_t i = 0; i < connections.size(); i++) {
        const auto& conn = connections[i];
        int v1 = conn.GetV1();
        int v2 = conn.GetV2();
        float dist = conn.distance();

        graph[v1].push_back({ v2, dist });
        graph[v2].push_back({ v1, dist });

        connectionMap[{v1, v2}] = (int)i;
        connectionMap[{v2, v1}] = (int)i;

        connectionIndexMap[v1][v2] = (int)i;
        connectionIndexMap[v2][v1] = (int)i;
    }

    const int START_VIRTUAL = -1;
    const int END_VIRTUAL = -2;

    // 添加起点到节点的虚拟道路到图
    for (const auto& road : startRoads) {
        const Connection& connection = road.first;
        float position = road.second;

        int v1 = connection.GetV1();
        int v2 = connection.GetV2();

        float dist_v1 = position * connection.distance();
        float dist_v2 = (1.0f - position) * connection.distance();

        graph[START_VIRTUAL].push_back({ v1, dist_v1 });
        graph[START_VIRTUAL].push_back({ v2, dist_v2 });
        graph[v1].push_back({ START_VIRTUAL, dist_v1 });
        graph[v2].push_back({ START_VIRTUAL, dist_v2 });
    }

    // 添加终点到节点的虚拟道路到图
    for (const auto& road : endRoads) {
        const Connection& connection = road.first;
        float position = road.second;

        int v1 = connection.GetV1();
        int v2 = connection.GetV2();

        float dist_v1 = position * connection.distance();
        float dist_v2 = (1.0f - position) * connection.distance();

        graph[v1].push_back({ END_VIRTUAL, dist_v1 });
        graph[v2].push_back({ END_VIRTUAL, dist_v2 });
        graph[END_VIRTUAL].push_back({ v1, dist_v1 });
        graph[END_VIRTUAL].push_back({ v2, dist_v2 });
    }

    // Dijkstra算法
    unordered_map<int, float> dist;
    unordered_map<int, int> prev;
    unordered_map<int, int> prevConnection;
    for (const auto& node : graph) {
        dist[node.first] = numeric_limits<float>::max();
    }
    dist[START_VIRTUAL] = 0.0f;

    using QueueItem = pair<float, int>;
    priority_queue<QueueItem, vector<QueueItem>, greater<QueueItem>> pq;
    pq.push({ 0.0f, START_VIRTUAL });

    while (!pq.empty()) {
        auto [currentDist, currentNode] = pq.top();
        pq.pop();

        if (currentDist > dist[currentNode]) {
            continue;
        }

        if (currentNode == END_VIRTUAL) break;

        for (const auto& neighbor : graph[currentNode]) {
            int neighborId = neighbor.first;
            float weight = neighbor.second;

            float newDist = currentDist + weight;

            if (newDist < dist[neighborId]) {
                dist[neighborId] = newDist;
                prev[neighborId] = currentNode;

                if (currentNode != START_VIRTUAL && neighborId != END_VIRTUAL) {
                    auto it = connectionIndexMap[currentNode].find(neighborId);
                    if (it != connectionIndexMap[currentNode].end()) {
                        prevConnection[neighborId] = it->second;
                    }
                }

                pq.push({ newDist, neighborId });
            }
        }
    }
    if (dist[END_VIRTUAL] == numeric_limits<float>::max())return {};

    // 提取路线
    vector<int> nodePath;
    int current = END_VIRTUAL;
    while (current != START_VIRTUAL) {
        nodePath.push_back(current);
        current = prev[current];
    }
    nodePath.push_back(START_VIRTUAL);
    reverse(nodePath.begin(), nodePath.end());

    if (nodePath.size() < 3) {
        THROW_EXCEPTION(RuntimeException, "auto navigation crash.\n");
    }

    vector<Connection> path;
    int firstRealNode = nodePath[1];
    for (const auto& road : startRoads) {
        const Connection& connection = road.first;
        if (connection.GetV1() == firstRealNode || connection.GetV2() == firstRealNode) {
            float begin = road.second;
            float end = (connection.GetV1() == firstRealNode) ? 0.0f : 1.0f;

            path.push_back(Connection(road.first.GetRoadnet(),
                connection.GetV1(),
                connection.GetV2(),
                begin, end));
            break;
        }
    }
    for (size_t i = 2; i < nodePath.size() - 1; i++) {
        int from = nodePath[i - 1];
        int to = nodePath[i];

        auto it = connectionMap.find({ from, to });
        if (it != connectionMap.end()) {
            const Connection& original = connections[it->second];
            path.push_back(Connection(original.GetRoadnet(),
                original.GetV1(),
                original.GetV2(),
                0.0f, 1.0f));
        }
    }
    int lastRealNode = nodePath[nodePath.size() - 2];
    for (const auto& road : endRoads) {
        const Connection& connection = road.first;
        if (connection.GetV1() == lastRealNode || connection.GetV2() == lastRealNode) {
            float begin = (connection.GetV1() == lastRealNode) ? 0.0f : 1.0f;
            float end = road.second;

            path.push_back(Connection(road.first.GetRoadnet(),
                connection.GetV1(),
                connection.GetV2(),
                begin, end));
            break;
        }
    }

    return path;
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
