#pragma once

#include "../map/zone_base.h"
#include "../map/building_base.h"

#include "utility.h"
#include "rect.h"

#include <vector>
#include <unordered_map>


class Zone;
class Building;

class Node {
public:
	Node();
	Node(float x, float y);
	~Node() = default;

	float GetX() const;
	float GetY() const;

private:
	float posX, posY;
};

struct NodeHash {
	std::size_t operator()(const Node& node) const {
		std::size_t h1 = std::hash<float>()(node.GetX());
		std::size_t h2 = std::hash<float>()(node.GetY());
		return h1 ^ (h2 << 1);
	}
};

struct NodeEqual {
	bool operator()(const Node& lhs, const Node& rhs) const {
		return abs(lhs.GetX() - rhs.GetX()) < FLT_EPSILON && abs(lhs.GetY() - rhs.GetY() < FLT_EPSILON);
	}
};

class Connection {
public:
	Connection();
	Connection(Node n1, Node n2);
	~Connection() = default;

	float distance() const;

	Node GetV1() const;
	Node GetV2() const;

private:
	std::pair<Node, Node> vertices;
};

enum AREA_TYPE {
	AREA_GREEN,
	AREA_RESIDENTIAL_HIGH,
	AREA_RESIDENTIAL_MIDDLE,
	AREA_RESIDENTIAL_LOW,
	AREA_COMMERCIAL_HIGH,
	AREA_COMMERCIAL_MIDDLE,
	AREA_COMMERCIAL_LOW,
	AREA_INDUSTRIAL_HIGH,
	AREA_INDUSTRIAL_MIDDLE,
	AREA_INDUSTRIAL_LOW,
	AREA_OFFICIAL_HIGH,
	AREA_OFFICIAL_MIDDLE,
	AREA_OFFICIAL_LOW,
};

class Plot : public Rect {
public:
	Plot();
	Plot(float x, float y, float w, float h, float r);
	Plot(Node n1, Node n2, Node n3);
	Plot(Node n1, Node n2, Node n3, Node n4);

	// 获取/设置属性
	float GetRotation() const;
	void SetRotation(float r);
	AREA_TYPE GetArea() const;
	void SetArea(AREA_TYPE area);
	std::vector<Connection> GetRoads();
	void SetRoads(std::vector<Connection> roads);

	// 世界坐标变换
	std::pair<float, float> GetVertex(int idx) const;
	std::pair<float, float> GetPosition(float x, float y) const;

	// 通过逆时针顺序三个顶点设置矩形
	void SetPosition(Node n1, Node n2, Node n3);

	// 通过顺序无关四个顶点设置矩形
	void SetPosition(Node n1, Node n2, Node n3, Node n4);

	// 内部Rect管理
	const std::vector<std::pair<std::string, std::shared_ptr<Zone>>> GetZones() const;
	const std::vector<std::pair<std::string, std::shared_ptr<Building>>>GetBuildings() const;
	void AddZone(std::string name, std::shared_ptr<Zone> zone);
	void AddBuilding(std::string name, std::shared_ptr<Building> building);
	std::shared_ptr<Zone> GetZone(std::string name) const;
	std::shared_ptr<Building> GetBuilding(std::string name) const;
	void RemoveZone(std::string name);
	void RemoveBuilding(std::string name);

protected:
	float rotation;
	AREA_TYPE area;

	std::vector<Connection> roads;

	std::vector<std::pair<std::string, std::shared_ptr<Zone>>> zones;
	std::vector<std::pair<std::string, std::shared_ptr<Building>>> buildings;
};
