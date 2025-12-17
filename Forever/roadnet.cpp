#include "roadnet.h"


using namespace std;

std::string JingRoadnet::GetId() {
    return "jing";
}

std::string JingRoadnet::GetType() const {
    return "jing";
}

std::string JingRoadnet::GetName() const {
    return "井字路网";
}

void JingRoadnet::DistributeRoadnet(int width, int height,
    std::function<std::string(int, int)> get) {
    vector<Node> horizontalNode1w;
    vector<Node> horizontalNode1e;
    vector<Node> horizontalNode2w;
    vector<Node> horizontalNode2e;
    vector<Node> verticalNode1n;
    vector<Node> verticalNode1s;
    vector<Node> verticalNode2n;
    vector<Node> verticalNode2s;

    float theta = GetRandom(1000) / 1000.f * 0.4f - 0.2f;

    Node northWest(width / 2.f + 32.f * (sin(theta) - cos(theta)), height / 2.f + 32.f * (-cos(theta) - sin(theta)));
    Node northEast(width / 2.f + 32.f * (sin(theta) + cos(theta)), height / 2.f + 32.f * (-cos(theta) + sin(theta)));
    Node southWest(width / 2.f + 32.f * (-sin(theta) - cos(theta)), height / 2.f + 32.f * (cos(theta) - sin(theta)));
    Node southEast(width / 2.f + 32.f * (-sin(theta) + cos(theta)), height / 2.f + 32.f * (cos(theta) + sin(theta)));

    float s = northEast.GetY() - northWest.GetY();
    float c = northEast.GetX() - northWest.GetX();
    
    for (float x = northWest.GetX(), y = northWest.GetY(); get((int)x, (int)y) != ""; x -= c, y -= s) {
        horizontalNode1w.emplace_back(x, y);
    }
    for (float x = northEast.GetX(), y = northEast.GetY(); get((int)x, (int)y) != ""; x += c, y += s) {
        horizontalNode1e.emplace_back(x, y);
    }
    for (float x = southWest.GetX(), y = southWest.GetY(); get((int)x, (int)y) != ""; x -= c, y -= s) {
        horizontalNode2w.emplace_back(x, y);
    }
    for (float x = southEast.GetX(), y = southEast.GetY(); get((int)x, (int)y) != ""; x += c, y += s) {
        horizontalNode2e.emplace_back(x, y);
    }
    for (float x = northWest.GetX(), y = northWest.GetY(); get((int)x, (int)y) != ""; x += s, y -= c) {
        verticalNode1n.emplace_back(x, y);
    }
    for (float x = southWest.GetX(), y = southWest.GetY(); get((int)x, (int)y) != ""; x -= s, y += c) {
        verticalNode1s.emplace_back(x, y);
    }
    for (float x = northEast.GetX(), y = northEast.GetY(); get((int)x, (int)y) != ""; x += s, y -= c) {
        verticalNode2n.emplace_back(x, y);
    }
    for (float x = southEast.GetX(), y = southEast.GetY(); get((int)x, (int)y) != ""; x -= s, y += c) {
        verticalNode2s.emplace_back(x, y);
    }

    connections.push_back(Connection(northWest, northEast));
    connections.push_back(Connection(northEast, southEast));
    connections.push_back(Connection(southEast, southWest));
    connections.push_back(Connection(southWest, northWest));
    for (int i = 1; i < horizontalNode1w.size(); i++) {
        connections.push_back(Connection(horizontalNode1w[i], horizontalNode1w[i - 1]));
    }
    for (int i = 1; i < horizontalNode1e.size(); i++) {
        connections.push_back(Connection(horizontalNode1e[i], horizontalNode1e[i - 1]));
    }
    for (int i = 1; i < horizontalNode2w.size(); i++) {
        connections.push_back(Connection(horizontalNode2w[i], horizontalNode2w[i - 1]));
    }
    for (int i = 1; i < horizontalNode2e.size(); i++) {
        connections.push_back(Connection(horizontalNode2e[i], horizontalNode2e[i - 1]));
    }
    for (int i = 1; i < verticalNode1n.size(); i++) {
        connections.push_back(Connection(verticalNode1n[i], verticalNode1n[i - 1]));
    }
    for (int i = 1; i < verticalNode1s.size(); i++) {
        connections.push_back(Connection(verticalNode1s[i], verticalNode1s[i - 1]));
    }
    for (int i = 1; i < verticalNode2n.size(); i++) {
        connections.push_back(Connection(verticalNode2n[i], verticalNode2n[i - 1]));
    }
    for (int i = 1; i < verticalNode2s.size(); i++) {
        connections.push_back(Connection(verticalNode2s[i], verticalNode2s[i - 1]));
    }

    plots.push_back(std::make_shared<Plot>(northWest, northEast, southEast, southWest));
    plots.back()->SetArea(AREA_RESIDENTIAL_HIGH);
    for (int i = 1; i < min(horizontalNode1w.size(), horizontalNode2w.size()); i++) {
        if (get(horizontalNode1w[i].GetX(), horizontalNode1w[i].GetY()) != "plain")continue;
        if (get(horizontalNode1w[i - i].GetX(), horizontalNode1w[i - 1].GetY()) != "plain")continue;
        if (get(horizontalNode2w[i].GetX(), horizontalNode2w[i].GetY()) != "plain")continue;
        if (get(horizontalNode2w[i - 1].GetX(), horizontalNode2w[i - 1].GetY()) != "plain")continue;
        plots.push_back(std::make_shared<Plot>(
            horizontalNode1w[i], horizontalNode1w[i - 1], horizontalNode2w[i - 1], horizontalNode2w[i]));
        plots.back()->SetArea(AREA_RESIDENTIAL_HIGH);
    }
    for (int i = 1; i < min(horizontalNode1e.size(), horizontalNode2e.size()); i++) {
        if (get(horizontalNode1e[i].GetX(), horizontalNode1e[i].GetY()) != "plain")continue;
        if (get(horizontalNode1e[i - i].GetX(), horizontalNode1e[i - 1].GetY()) != "plain")continue;
        if (get(horizontalNode2e[i].GetX(), horizontalNode2e[i].GetY()) != "plain")continue;
        if (get(horizontalNode2e[i - 1].GetX(), horizontalNode2e[i - 1].GetY()) != "plain")continue;
        plots.push_back(std::make_shared<Plot>(
            horizontalNode1e[i - 1], horizontalNode1e[i], horizontalNode2e[i], horizontalNode2e[i - 1]));
        plots.back()->SetArea(AREA_RESIDENTIAL_HIGH);
    }
    for (int i = 1; i < min(verticalNode1n.size(), verticalNode2n.size()); i++) {
        if (get(verticalNode1n[i].GetX(), verticalNode1n[i].GetY()) != "plain")continue;
        if (get(verticalNode1n[i - i].GetX(), verticalNode1n[i - 1].GetY()) != "plain")continue;
        if (get(verticalNode2n[i].GetX(), verticalNode2n[i].GetY()) != "plain")continue;
        if (get(verticalNode2n[i - 1].GetX(), verticalNode2n[i - 1].GetY()) != "plain")continue;
        plots.push_back(std::make_shared<Plot>(
            verticalNode1n[i], verticalNode2n[i], verticalNode2n[i - 1], verticalNode1n[i - 1]));
        plots.back()->SetArea(AREA_RESIDENTIAL_HIGH);
    }
    for (int i = 1; i < min(verticalNode1s.size(), verticalNode2s.size()); i++) {
        if (get(verticalNode1s[i].GetX(), verticalNode1s[i].GetY()) != "plain")continue;
        if (get(verticalNode1s[i - i].GetX(), verticalNode1s[i - 1].GetY()) != "plain")continue;
        if (get(verticalNode2s[i].GetX(), verticalNode2s[i].GetY()) != "plain")continue;
        if (get(verticalNode2s[i - 1].GetX(), verticalNode2s[i - 1].GetY()) != "plain")continue;
        plots.push_back(std::make_shared<Plot>(
            verticalNode1s[i - 1], verticalNode2s[i - 1], verticalNode2s[i], verticalNode1s[i]));
        plots.back()->SetArea(AREA_RESIDENTIAL_HIGH);
    }
}
