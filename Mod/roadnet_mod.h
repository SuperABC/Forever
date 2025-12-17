#pragma once

#include "roadnet_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModRoadnet : public Roadnet {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetType() const override { return "mod"; }
    virtual std::string GetName() const override { return "模组路网"; }

    virtual void DistributeRoadnet(int width, int height,
        std::function<std::string(int, int)> get) override {
        Node n1 = Node(width / 2.f + 32.f, height / 2.f + 32.f);
        Node n2 = Node(width / 2.f - 32.f, height / 2.f + 32.f);
        Node n3 = Node(width / 2.f - 32.f, height / 2.f - 32.f);
        Node n4 = Node(width / 2.f + 32.f, height / 2.f - 32.f);
        nodes.push_back(n1);
        nodes.push_back(n2);
        nodes.push_back(n3);
        nodes.push_back(n4);
        connections.push_back(Connection(n1, n2));
        connections.push_back(Connection(n2, n3));
        connections.push_back(Connection(n3, n4));
        connections.push_back(Connection(n4, n1));
        plots.push_back(std::make_shared<Plot>(n1, n2, n3, n4));
        plots.back()->SetArea(AREA_GREEN);
    }
};

