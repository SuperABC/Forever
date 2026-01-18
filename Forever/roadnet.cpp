#include "roadnet.h"


using namespace std;

string JingRoadnet::GetId() {
    return "jing";
}

string JingRoadnet::GetType() const {
    return "jing";
}

string JingRoadnet::GetName() const {
    return "井字路网";
}

void JingRoadnet::DistributeRoadnet(std::shared_ptr<Roadnet> self, int width, int height,
    function<string(int, int)> get) {
    nodes.clear();
    connections.clear();
    plots.clear();

    vector<pair<Node, int>> horizontalNode1w;
    vector<pair<Node, int>> horizontalNode1e;
    vector<pair<Node, int>> horizontalNode2w;
    vector<pair<Node, int>> horizontalNode2e;
    vector<pair<Node, int>> verticalNode1n;
    vector<pair<Node, int>> verticalNode1s;
    vector<pair<Node, int>> verticalNode2n;
    vector<pair<Node, int>> verticalNode2s;

    float theta = GetRandom(1000) / 1000.f * 0.4f - 0.2f;

    Node northWest(width / 2.f + 32.f * (sin(theta) - cos(theta)), height / 2.f + 32.f * (-cos(theta) - sin(theta)));
    Node northEast(width / 2.f + 32.f * (sin(theta) + cos(theta)), height / 2.f + 32.f * (-cos(theta) + sin(theta)));
    Node southWest(width / 2.f + 32.f * (-sin(theta) - cos(theta)), height / 2.f + 32.f * (cos(theta) - sin(theta)));
    Node southEast(width / 2.f + 32.f * (-sin(theta) + cos(theta)), height / 2.f + 32.f * (cos(theta) + sin(theta)));
    nodes.push_back(northWest);
    nodes.push_back(northEast);
    nodes.push_back(southWest);
    nodes.push_back(southEast);

    float s = northEast.GetY() - northWest.GetY();
    float c = northEast.GetX() - northWest.GetX();
    
    for (float x = northWest.GetX(), y = northWest.GetY(); get((int)x, (int)y) != ""; x -= c, y -= s) {
        nodes.emplace_back(x, y);
        horizontalNode1w.emplace_back(nodes.back(), (int)nodes.size() - 1);
    }
    for (float x = northEast.GetX(), y = northEast.GetY(); get((int)x, (int)y) != ""; x += c, y += s) {
        nodes.emplace_back(x, y);
        horizontalNode1e.emplace_back(nodes.back(), (int)nodes.size() - 1);
    }
    for (float x = southWest.GetX(), y = southWest.GetY(); get((int)x, (int)y) != ""; x -= c, y -= s) {
        nodes.emplace_back(x, y);
        horizontalNode2w.emplace_back(nodes.back(), (int)nodes.size() - 1);
    }
    for (float x = southEast.GetX(), y = southEast.GetY(); get((int)x, (int)y) != ""; x += c, y += s) {
        nodes.emplace_back(x, y);
        horizontalNode2e.emplace_back(nodes.back(), (int)nodes.size() - 1);
    }
    for (float x = northWest.GetX(), y = northWest.GetY(); get((int)x, (int)y) != ""; x += s, y -= c) {
        nodes.emplace_back(x, y);
        verticalNode1n.emplace_back(nodes.back(), (int)nodes.size() - 1);
    }
    for (float x = southWest.GetX(), y = southWest.GetY(); get((int)x, (int)y) != ""; x -= s, y += c) {
        nodes.emplace_back(x, y);
        verticalNode1s.emplace_back(nodes.back(), (int)nodes.size() - 1);
    }
    for (float x = northEast.GetX(), y = northEast.GetY(); get((int)x, (int)y) != ""; x += s, y -= c) {
        nodes.emplace_back(x, y);
        verticalNode2n.emplace_back(nodes.back(), (int)nodes.size() - 1);
    }
    for (float x = southEast.GetX(), y = southEast.GetY(); get((int)x, (int)y) != ""; x -= s, y += c) {
        nodes.emplace_back(x, y);
        verticalNode2s.emplace_back(nodes.back(), (int)nodes.size() - 1);
    }

    connections.push_back(Connection(self, 0, 1));
    connections.push_back(Connection(self, 1, 3));
    connections.push_back(Connection(self, 3, 2));
    connections.push_back(Connection(self, 2, 0));
    for (int i = 1; i < horizontalNode1w.size(); i++) {
        connections.push_back(Connection(self, horizontalNode1w[i].second, horizontalNode1w[i - 1].second));
    }
    for (int i = 1; i < horizontalNode1e.size(); i++) {
        connections.push_back(Connection(self, horizontalNode1e[i].second, horizontalNode1e[i - 1].second));
    }
    for (int i = 1; i < horizontalNode2w.size(); i++) {
        connections.push_back(Connection(self, horizontalNode2w[i].second, horizontalNode2w[i - 1].second));
    }
    for (int i = 1; i < horizontalNode2e.size(); i++) {
        connections.push_back(Connection(self, horizontalNode2e[i].second, horizontalNode2e[i - 1].second));
    }
    for (int i = 1; i < verticalNode1n.size(); i++) {
        connections.push_back(Connection(self, verticalNode1n[i].second, verticalNode1n[i - 1].second));
    }
    for (int i = 1; i < verticalNode1s.size(); i++) {
        connections.push_back(Connection(self, verticalNode1s[i].second, verticalNode1s[i - 1].second));
    }
    for (int i = 1; i < verticalNode2n.size(); i++) {
        connections.push_back(Connection(self, verticalNode2n[i].second, verticalNode2n[i - 1].second));
    }
    for (int i = 1; i < verticalNode2s.size(); i++) {
        connections.push_back(Connection(self, verticalNode2s[i].second, verticalNode2s[i - 1].second));
    }

    plots.push_back(make_shared<Plot>(northWest, northEast, southEast, southWest));
    plots.back()->SetArea(AREA_RESIDENTIAL_HIGH);
    for (int i = 1; i < min(horizontalNode1w.size(), horizontalNode2w.size()); i++) {
        if (get((int)horizontalNode1w[i].first.GetX(), (int)horizontalNode1w[i].first.GetY()) != "plain")continue;
        if (get((int)horizontalNode1w[i - 1].first.GetX(), (int)horizontalNode1w[i - 1].first.GetY()) != "plain")continue;
        if (get((int)horizontalNode2w[i].first.GetX(), (int)horizontalNode2w[i].first.GetY()) != "plain")continue;
        if (get((int)horizontalNode2w[i - 1].first.GetX(), (int)horizontalNode2w[i - 1].first.GetY()) != "plain")continue;
        plots.push_back(make_shared<Plot>(
            horizontalNode1w[i].first, horizontalNode1w[i - 1].first, horizontalNode2w[i - 1].first, horizontalNode2w[i].first));
        plots.back()->SetArea(AREA_RESIDENTIAL_HIGH);
        plots.back()->SetRoads({
            Connection(self, horizontalNode1w[i].second, horizontalNode1w[i - 1].second),
            Connection(self, horizontalNode2w[i].second, horizontalNode2w[i - 1].second) });
    }
    for (int i = 1; i < min(horizontalNode1e.size(), horizontalNode2e.size()); i++) {
        if (get((int)horizontalNode1e[i].first.GetX(), (int)horizontalNode1e[i].first.GetY()) != "plain")continue;
        if (get((int)horizontalNode1e[i - 1].first.GetX(), (int)horizontalNode1e[i - 1].first.GetY()) != "plain")continue;
        if (get((int)horizontalNode2e[i].first.GetX(), (int)horizontalNode2e[i].first.GetY()) != "plain")continue;
        if (get((int)horizontalNode2e[i - 1].first.GetX(), (int)horizontalNode2e[i - 1].first.GetY()) != "plain")continue;
        plots.push_back(make_shared<Plot>(
            horizontalNode1e[i - 1].first, horizontalNode1e[i].first, horizontalNode2e[i].first, horizontalNode2e[i - 1].first));
        plots.back()->SetArea(AREA_RESIDENTIAL_HIGH);
        plots.back()->SetRoads({
            Connection(self, horizontalNode1e[i].second, horizontalNode1e[i - 1].second),
            Connection(self, horizontalNode2e[i].second, horizontalNode2e[i - 1].second) });
    }
    for (int i = 1; i < min(verticalNode1n.size(), verticalNode2n.size()); i++) {
        if (get((int)verticalNode1n[i].first.GetX(), (int)verticalNode1n[i].first.GetY()) != "plain")continue;
        if (get((int)verticalNode1n[i - 1].first.GetX(), (int)verticalNode1n[i - 1].first.GetY()) != "plain")continue;
        if (get((int)verticalNode2n[i].first.GetX(), (int)verticalNode2n[i].first.GetY()) != "plain")continue;
        if (get((int)verticalNode2n[i - 1].first.GetX(), (int)verticalNode2n[i - 1].first.GetY()) != "plain")continue;
        plots.push_back(make_shared<Plot>(
            verticalNode1n[i].first, verticalNode2n[i].first, verticalNode2n[i - 1].first, verticalNode1n[i - 1].first));
        plots.back()->SetArea(AREA_RESIDENTIAL_HIGH);
        plots.back()->SetRoads({
            Connection(self, verticalNode1n[i].second, verticalNode1n[i - 1].second),
            Connection(self, verticalNode2n[i].second, verticalNode2n[i - 1].second) });
    }
    for (int i = 1; i < min(verticalNode1s.size(), verticalNode2s.size()); i++) {
        if (get((int)verticalNode1s[i].first.GetX(), (int)verticalNode1s[i].first.GetY()) != "plain")continue;
        if (get((int)verticalNode1s[i - 1].first.GetX(), (int)verticalNode1s[i - 1].first.GetY()) != "plain")continue;
        if (get((int)verticalNode2s[i].first.GetX(), (int)verticalNode2s[i].first.GetY()) != "plain")continue;
        if (get((int)verticalNode2s[i - 1].first.GetX(), (int)verticalNode2s[i - 1].first.GetY()) != "plain")continue;
        plots.push_back(make_shared<Plot>(
            verticalNode1s[i - 1].first, verticalNode2s[i - 1].first, verticalNode2s[i].first, verticalNode1s[i].first));
        plots.back()->SetArea(AREA_RESIDENTIAL_HIGH);
        plots.back()->SetRoads({
            Connection(self, verticalNode1s[i].second, verticalNode1s[i - 1].second),
            Connection(self, verticalNode2s[i].second, verticalNode2s[i - 1].second) });
    }
}
