#pragma once

#include "../common/plot.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Roadnet {
public:
    Roadnet() = default;
    virtual ~Roadnet() = default;

	// 子类实现方法

    // 动态返回路网名称
    static std::string GetId();
    virtual std::string GetType() const = 0;
    virtual std::string GetName() const = 0;

    // 在平原上生成路网
    virtual void DistributeRoadnet(int width, int height,
        std::function<std::string(int, int)> get) = 0;

	// 父类实现方法

    // 提供只读访问接口
    const std::vector<Node>& GetNodes() const;
    const std::vector<Connection>& GetConnections() const;
    const std::vector<std::shared_ptr<Plot>>& GetPlots() const;

    // 构建或重建邻接表
    void BuildAdjacency();

    // 自动寻路
    const std::vector<std::pair<Connection, Node>> AutoNavigate(
        std::shared_ptr<Plot> start, std::shared_ptr<Plot> end) const;

protected:
    std::vector<Node> nodes;
    std::vector<Connection> connections;
    std::vector<std::shared_ptr<Plot>> plots;

private:
    mutable std::unordered_map<Node, std::vector<std::pair<Node, Connection>>, NodeHash, NodeEqual> adjacencyList;
    mutable bool adjacencyFlag = true;

    const std::unordered_map<Node, std::vector<std::pair<Node, Connection>>, NodeHash, NodeEqual>&
        GetAdjacency() const;

    std::vector<Node> BFSPath(const Node& start, const Node& end) const;
};

class RoadnetFactory {
public:
    void RegisterRoadnet(const std::string& id, std::function<std::unique_ptr<Roadnet>()> creator);
    std::unique_ptr<Roadnet> CreateRoadnet(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);
    std::unique_ptr<Roadnet> GetRoadnet() const;

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Roadnet>()>> registries;
    std::unordered_map<std::string, bool> configs;
};