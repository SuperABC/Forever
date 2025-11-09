#include "error.h"
#include "plot.h"

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

Connection::Connection(Node n1, Node n2) : vertices(n1, n2) {

}

Node Connection::GetV1() const {
    return vertices.first;
}

Node Connection::GetV2() const {
    return vertices.second;
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

pair<float, float> Plot::GetVertex(int idx) {
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
    if (std::abs(dot) > eps) {
        THROW_EXCEPTION(InvalidArgumentException, "The given edges are not perpendicular.\n");
    }

    // 计算尺寸
    float sx = std::sqrt(ux * ux + uy * uy);
    float sy = std::sqrt(vx * vx + vy * vy);

    // 计算中心点：p1 和 p3 的对角线中心
    float cx = (x1 + x3) / 2.0f;
    float cy = (y1 + y3) / 2.0f;

    // 计算旋转角度（p1p2 与 x 轴夹角）
    float rot = std::atan2(uy, ux);

    // 更新成员变量
    posX = cx;
    posY = cy;
    sizeX = sx;
    sizeY = sy;
    rotation = rot;
    acreage = sx * sy * 100.f;
}
void Plot::SetPosition(Node n1, Node n2, Node n3, Node n4) {
    std::vector<Node> nodes = { n1, n2, n3, n4 };

    // 计算中心点
    float cx = 0.0f, cy = 0.0f;
    for (const auto& node : nodes) {
        cx += node.GetX();
        cy += node.GetY();
    }
    cx /= 4.0f;
    cy /= 4.0f;

    // 按极角排序（逆时针）
    std::sort(nodes.begin(), nodes.end(),
        [cx, cy](const Node& a, const Node& b) {
            return std::atan2(a.GetY() - cy, a.GetX() - cx) <
                std::atan2(b.GetY() - cy, b.GetX() - cx);
        });

    // 现在 nodes[0], nodes[1], nodes[2], nodes[3] 是逆时针顺序

    // 检查矩形条件
    const float eps = 1e-5f;

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

    if (std::abs(dot1) > eps || std::abs(dot2) > eps ||
        std::abs(dot3) > eps || std::abs(dot4) > eps) {
        THROW_EXCEPTION(InvalidArgumentException, "The given points do not form a rectangle (not all angles are 90 degrees).\n");
    }

    // 检查对边长度相等
    float len1 = std::sqrt(u1x * u1x + u1y * u1y);
    float len2 = std::sqrt(u2x * u2x + u2y * u2y);
    float len3 = std::sqrt(u3x * u3x + u3y * u3y);
    float len4 = std::sqrt(u4x * u4x + u4y * u4y);

    if (std::abs(len1 - len3) > eps || std::abs(len2 - len4) > eps) {
        THROW_EXCEPTION(InvalidArgumentException, "The given points do not form a rectangle (opposite sides not equal).\n");
    }

    // 计算尺寸（取相邻两边长度）
    float sx = len1;
    float sy = len2;

    // 计算旋转角度（使用第一条边 nodes[0]->nodes[1]）
    float rot = std::atan2(u1y, u1x);

    // 更新成员变量
    posX = cx;
    posY = cy;
    sizeX = sx;
    sizeY = sy;
    rotation = rot;
    acreage = sx * sy * 100.f;
}

const std::vector<std::pair<std::string, std::shared_ptr<Zone>>> Plot::GetZones() const {
    return zones;
}

const std::vector<std::pair<std::string, std::shared_ptr<Building>>> Plot::GetBuildings() const {
    return buildings;
}

void Plot::AddZone(std::string name, std::shared_ptr<Zone> zone) {
    zones.push_back(make_pair(name, zone));
}

void Plot::AddBuilding(std::string name, std::shared_ptr<Building> building) {
    buildings.push_back(make_pair(name, building));
}

std::shared_ptr<Zone> Plot::GetZone(std::string name) const {
    for (auto& zone : zones) {
        if(zone.first == name){
            return zone.second;
        }
    }
    return nullptr;
}

std::shared_ptr<Building> Plot::GetBuilding(std::string name) const {
    for (auto& building : buildings) {
        if (building.first == name) {
            return building.second;
        }
    }
    return nullptr;
}

void Plot::RemoveZone(std::string name) {
    for (auto it = zones.begin(); it != zones.end(); ) {
        if(it->first == name){
            it = zones.erase(it);
        }
        else {
            ++it;
        }
    }
}

void Plot::RemoveBuilding(std::string name) {
    for (auto it = buildings.begin(); it != buildings.end(); ) {
        if (it->first == name) {
            it = buildings.erase(it);
        }
        else {
            ++it;
        }
    }
}
