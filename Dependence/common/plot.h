#pragma once

#include "../map/zone_base.h"
#include "../map/building_base.h"

#include "util.h"
#include "rect.h"

#include <vector>
#include <unordered_map>


class Zone;
class Building;

class Node {
public:
	Node(float x, float y);
	~Node() = default;

	float GetX() const;
	float GetY() const;

private:
	float posX, posY;
};

class Connection {
public:
	Connection(Node n1, Node n2);
	~Connection() = default;

	Node GetV1() const;
	Node GetV2() const;

private:
	std::pair<Node, Node> vertices;
};

class Plot : public Rect {
public:
	Plot() : Rect(), rotation(0.f), acreage(0.f) {}
	Plot(float x, float y, float w, float h, float r) : Rect(x, y, w, h), rotation(r), acreage(w * h * 100.f) {}
	Plot(Node n1, Node n2, Node n3) { SetPosition(n1, n2, n3); }
	Plot(Node n1, Node n2, Node n3, Node n4) { SetPosition(n1, n2, n3, n4); }

	// 获取属性
	float GetRotation();
	void SetRotation(float r);
	std::pair<float, float> GetVertex(int idx);

	// 通过逆时针顺序三个顶点设置矩形
	void SetPosition(Node n1, Node n2, Node n3);

	// 通过顺序无关四个顶点设置矩形
	void SetPosition(Node n1, Node n2, Node n3, Node n4);

	// 内部Rect管理
	void AddZone(std::string name, std::shared_ptr<Zone> zone);
	void AddBuilding(std::string name, std::shared_ptr<Building> building);
	std::shared_ptr<Zone> GetZone(std::string name);
	std::shared_ptr<Building> GetBuilding(std::string name);
	void RemoveZone(std::string name);
	void RemoveBuilding(std::string name);

protected:
	float rotation;
	float acreage;

	std::vector<std::pair<std::string, std::shared_ptr<Zone>>> zones;
	std::vector<std::pair<std::string, std::shared_ptr<Building>>> buildings;
};
