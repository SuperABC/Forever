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

void Roadnet::BuildAdjacency() {
    adjacencyList.clear();

    // 构建邻接表
    for (const auto& connection : connections) {
        Node n1 = connection.GetV1();
        Node n2 = connection.GetV2();

        adjacencyList[n1].emplace_back(n2, connection);
        adjacencyList[n2].emplace_back(n1, connection);
    }

    adjacencyFlag = false;
}

const std::vector<std::pair<Connection, Node>> Roadnet::AutoNavigate(
    std::shared_ptr<Plot> start, std::shared_ptr<Plot> end) const {

    std::vector<std::pair<Connection, Node>> result;

    // 参数检查
    if (!start || !end) {
        return result;
    }

    // 获取起点和终点的相邻道路
    std::vector<Connection> startRoads = start->GetRoads();
    std::vector<Connection> endRoads = end->GetRoads();

    if (startRoads.empty() || endRoads.empty()) {
        return result;
    }

    // 选择起点和终点节点（简化处理：选择每条道路的第一个节点）
    // 在实际应用中，可能需要选择离Plot最近的点
    Node startNode = startRoads[0].GetV1();
    Node endNode = endRoads[0].GetV1();

    // 如果起点和终点相同
    if (NodeEqual{}(startNode, endNode)) {
        // 如果起点和终点使用同一条道路
        if (startRoads[0].GetV1().GetX() == endRoads[0].GetV1().GetX() &&
            startRoads[0].GetV1().GetY() == endRoads[0].GetV1().GetY()) {
            result.emplace_back(startRoads[0], endNode);
        }
        return result;
    }

    // 使用BFS找到节点路径
    std::vector<Node> nodePath = BFSPath(startNode, endNode);

    if (nodePath.empty()) {
        return result; // 没有找到路径
    }

    // 将节点路径转换为连接路径
    const auto& adj = GetAdjacency();

    for (size_t i = 0; i < nodePath.size() - 1; ++i) {
        Node current = nodePath[i];
        Node next = nodePath[i + 1];

        // 查找连接当前节点和下一个节点的Connection
        const auto& neighbors = adj.at(current);

        for (const auto& neighbor : neighbors) {
            if (NodeEqual{}(neighbor.first, next)) {
                // 找到连接
                Connection conn = neighbor.second;

                // 确保连接的方向正确（终点是next）
                Node connV1 = conn.GetV1();
                Node connV2 = conn.GetV2();

                if (NodeEqual{}(connV1, current) && NodeEqual {}(connV2, next)) {
                    // 方向正确
                    result.emplace_back(conn, next);
                }
                else if (NodeEqual{}(connV1, next) && NodeEqual {}(connV2, current)) {
                    // 方向相反，需要反转连接
                    Connection reversedConn(connV2, connV1);
                    result.emplace_back(reversedConn, next);
                }
                else {
                    // 这不应该发生，但为了安全起见
                    result.emplace_back(conn, next);
                }
                break;
            }
        }
    }

    return result;
}

const std::unordered_map<Node, std::vector<std::pair<Node, Connection>>, NodeHash, NodeEqual>&
Roadnet::GetAdjacency() const {
    if (adjacencyFlag || adjacencyList.empty()) {
        // 在const函数中重建邻接表需要去除const（但不修改nodes/connections）
        const_cast<Roadnet*>(this)->BuildAdjacency();
    }
    return adjacencyList;
}

std::vector<Node> Roadnet::BFSPath(const Node& start, const Node& end) const {
    const auto& adj = GetAdjacency();

    // 如果起点或终点不在邻接表中，返回空路径
    if (adj.find(start) == adj.end() || adj.find(end) == adj.end()) {
        return {};
    }

    // BFS数据结构
    std::unordered_map<Node, Node, NodeHash, NodeEqual> parent;
    std::unordered_map<Node, bool, NodeHash, NodeEqual> visited;
    std::queue<Node> q;

    // 初始化BFS
    q.push(start);
    visited[start] = true;
    parent[start] = start; // 起点标记

    bool found = false;

    // BFS搜索
    while (!q.empty() && !found) {
        Node current = q.front();
        q.pop();

        // 检查是否到达终点
        if (NodeEqual{}(current, end)) {
            found = true;
            break;
        }

        // 遍历邻居
        const auto& neighbors = adj.at(current);
        for (const auto& neighbor : neighbors) {
            Node next = neighbor.first;

            if (!visited[next]) {
                visited[next] = true;
                parent[next] = current;
                q.push(next);
            }
        }
    }

    // 重构路径
    std::vector<Node> path;
    if (found) {
        // 从终点回溯到起点
        Node current = end;
        while (!NodeEqual{}(current, start)) {
            path.push_back(current);
            current = parent[current];
        }
        path.push_back(start);

        // 反转路径得到从起点到终点的顺序
        std::reverse(path.begin(), path.end());
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
