#include "error.h"
#include "plot.h"
#include "../map/roadnet_base.h"

#include <cmath>
#include <utility>
#include <algorithm>


using namespace std;

Node::Node(float x, float y) : posX(x), posY(y) {

}

float Node::GetX() const {
    return posX;
}

float Node::GetY() const {
    return posY;
}

Connection::Connection(shared_ptr<Roadnet> roadnet, int n1, int n2, float begin, float end) :
    roadnet(roadnet), vertices(n1, n2), begin(begin), end(end) {

}

float Connection::distance() const {
    float dx = roadnet->GetNodes()[vertices.first].GetX() - roadnet->GetNodes()[vertices.second].GetX();
    float dy = roadnet->GetNodes()[vertices.first].GetY() - roadnet->GetNodes()[vertices.second].GetY();
    return std::sqrt(dx * dx + dy * dy);
}

bool Connection::operator==(Connection& other) {
    if (vertices.first == other.vertices.first && vertices.second == other.vertices.second)return true;
    if (vertices.first == other.vertices.second && vertices.second == other.vertices.first)return true;
    return false;
}

std::shared_ptr<Roadnet> Connection::GetRoadnet() const {
    return roadnet;
}

int Connection::GetV1() const {
    return vertices.first;
}

int Connection::GetV2() const {
    return vertices.second;
}

Plot::Plot() : Rect(), rotation(0.f) {

}

Plot::Plot(float x, float y, float w, float h, float r) : Rect(x, y, w, h), rotation(r) {

}

Plot::Plot(Node n1, Node n2, Node n3) {
    SetPosition(n1, n2, n3);
}

Plot::Plot(Node n1, Node n2, Node n3, Node n4) {
    SetPosition(n1, n2, n3, n4);
}

float Plot::GetRotation() const {
	return rotation;
}

void Plot::SetRotation(float r) {
    rotation = r;
}

AREA_TYPE Plot::GetArea() const {
    return area;
}

void Plot::SetArea(AREA_TYPE area) {
    this->area = area;
}

std::vector<std::pair<Connection, float>> Plot::GetRoads() {
    return roads;
}

void Plot::SetRoads(std::vector<std::pair<Connection, float>> roads) {
    this->roads = roads;
}

pair<float, float> Plot::GetVertex(int idx) const {
	if (idx < 1 || idx > 4) {
		THROW_EXCEPTION(InvalidArgumentException, "Plot has only 4 vertices.\n");
	}

    float hx = sizeX / 2.0f;
    float hy = sizeY / 2.0f;
    float c = cos(rotation);
    float s = sin(rotation);

    switch (idx) {
    case 1: // 右上
        return { posX + hx * c - hy * s,
                 posY + hx * s + hy * c };
    case 2: // 左上
        return { posX - hx * c - hy * s,
                 posY - hx * s + hy * c };
    case 3: // 左下
        return { posX - hx * c + hy * s,
                 posY - hx * s - hy * c };
    case 4: // 右下
        return { posX + hx * c + hy * s,
                 posY + hx * s - hy * c };
    default:
        return { 0.f, 0.f };
    }
}

pair<float, float> Plot::GetPosition(float x, float y) const {
    float cosR = cos(rotation);
    float sinR = sin(rotation);

    float originX = posX - sizeX / 2.0f;
    float originY = posY - sizeY / 2.0f;

    float relativeX = x - sizeX / 2.0f;
    float relativeY = y - sizeY / 2.0f;

    float rotatedX = relativeX * cosR - relativeY * sinR;
    float rotatedY = relativeX * sinR + relativeY * cosR;

    return { posX + rotatedX , posY + rotatedY };
}

void Plot::SetPosition(Node n1, Node n2, Node n3) {
    float x1 = n1.GetX(), y1 = n1.GetY();
    float x2 = n2.GetX(), y2 = n2.GetY();
    float x3 = n3.GetX(), y3 = n3.GetY();

    // 向量 u = p2 - p1, v = p3 - p2
    float ux = x2 - x1, uy = y2 - y1;
    float vx = x3 - x2, vy = y3 - y2;

    // 检查垂直
    float dot = ux * vx + uy * vy;
    const float eps = 1e-5f;
    if (abs(dot) > eps) {
        THROW_EXCEPTION(InvalidArgumentException, "The given edges are not perpendicular.\n");
    }

    // 计算尺寸
    float sx = sqrt(ux * ux + uy * uy);
    float sy = sqrt(vx * vx + vy * vy);

    // 计算中心点：p1 和 p3 的对角线中心
    float cx = (x1 + x3) / 2.0f;
    float cy = (y1 + y3) / 2.0f;

    // 计算旋转角度（p1p2 与 x 轴夹角）
    float rot = atan2(uy, ux);

    // 更新成员变量
    posX = cx;
    posY = cy;
    sizeX = sx;
    sizeY = sy;
    rotation = rot;
    acreage = sx * sy * 100.f;
}
void Plot::SetPosition(Node n1, Node n2, Node n3, Node n4) {
    vector<Node> nodes = { n1, n2, n3, n4 };

    // 计算中心点
    float cx = 0.0f, cy = 0.0f;
    for (const auto& node : nodes) {
        cx += node.GetX();
        cy += node.GetY();
    }
    cx /= 4.0f;
    cy /= 4.0f;

    // 按极角排序（逆时针）
    sort(nodes.begin(), nodes.end(),
        [cx, cy](const Node& a, const Node& b) {
            return atan2(a.GetY() - cy, a.GetX() - cx) <
                atan2(b.GetY() - cy, b.GetX() - cx);
        });

    // 现在 nodes[0], nodes[1], nodes[2], nodes[3] 是逆时针顺序

    // 检查矩形条件
    const float eps = 1e-2f;

    // 计算四个边向量
    float u1x = nodes[1].GetX() - nodes[0].GetX();
    float u1y = nodes[1].GetY() - nodes[0].GetY();
    float u2x = nodes[2].GetX() - nodes[1].GetX();
    float u2y = nodes[2].GetY() - nodes[1].GetY();
    float u3x = nodes[3].GetX() - nodes[2].GetX();
    float u3y = nodes[3].GetY() - nodes[2].GetY();
    float u4x = nodes[0].GetX() - nodes[3].GetX();
    float u4y = nodes[0].GetY() - nodes[3].GetY();

    // 检查邻边垂直（点积为0）
    float dot1 = u1x * u2x + u1y * u2y;
    float dot2 = u2x * u3x + u2y * u3y;
    float dot3 = u3x * u4x + u3y * u4y;
    float dot4 = u4x * u1x + u4y * u1y;

    if (abs(dot1) > eps || abs(dot2) > eps ||
        abs(dot3) > eps || abs(dot4) > eps) {
        THROW_EXCEPTION(InvalidArgumentException, "The given points do not form a rectangle (not all angles are 90 degrees).\n");
    }

    // 检查对边长度相等
    float len1 = sqrt(u1x * u1x + u1y * u1y);
    float len2 = sqrt(u2x * u2x + u2y * u2y);
    float len3 = sqrt(u3x * u3x + u3y * u3y);
    float len4 = sqrt(u4x * u4x + u4y * u4y);

    if (abs(len1 - len3) > eps || abs(len2 - len4) > eps) {
        THROW_EXCEPTION(InvalidArgumentException, "The given points do not form a rectangle (opposite sides not equal).\n");
    }

    // 计算尺寸（取相邻两边长度）
    float sx = len1;
    float sy = len2;

    // 计算旋转角度（使用第一条边 nodes[0]->nodes[1]）
    float rot = atan2(u1y, u1x);

    // 更新成员变量
    posX = cx;
    posY = cy;
    sizeX = sx;
    sizeY = sy;
    rotation = rot;
    acreage = sx * sy * 100.f;
}

const vector<pair<string, shared_ptr<Zone>>> Plot::GetZones() const {
    return zones;
}

const vector<pair<string, shared_ptr<Building>>> Plot::GetBuildings() const {
    return buildings;
}

void Plot::AddZone(string name, shared_ptr<Zone> zone) {
    zones.push_back(make_pair(name, zone));
}

void Plot::AddBuilding(string name, shared_ptr<Building> building) {
    buildings.push_back(make_pair(name, building));
}

shared_ptr<Zone> Plot::GetZone(string name) const {
    for (auto& zone : zones) {
        if(zone.first == name){
            return zone.second;
        }
    }
    return nullptr;
}

shared_ptr<Building> Plot::GetBuilding(string name) const {
    for (auto& building : buildings) {
        if (building.first == name) {
            return building.second;
        }
    }
    return nullptr;
}

void Plot::RemoveZone(string name) {
    for (auto it = zones.begin(); it != zones.end(); ) {
        if(it->first == name){
            it = zones.erase(it);
        }
        else {
            ++it;
        }
    }
}

void Plot::RemoveBuilding(string name) {
    for (auto it = buildings.begin(); it != buildings.end(); ) {
        if (it->first == name) {
            it = buildings.erase(it);
        }
        else {
            ++it;
        }
    }
}
