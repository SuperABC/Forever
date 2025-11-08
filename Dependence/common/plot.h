#pragma once

#include "util.h"

#include <vector>


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

class Plot {
public:
	Plot() : centerX(0.f), centerY(0.f), sizeX(0.f), sizeY(0.f), rotation(0.f), acreage(0.f) {}
	Plot(float x, float y, float w, float h, float r) { SetPosition(x, y, w, h, r); }
	Plot(Node n1, Node n2, Node n3) { SetPosition(n1, n2, n3); }
	Plot(Node n1, Node n2, Node n3, Node n4) { SetPosition(n1, n2, n3, n4); }

	// 获取属性
	float GetCenterX();
	float GetCenterY();
	float GetSizeX();
	float GetSizeY();
	float GetRotation();
	std::pair<float, float> GetVertex(int idx);

	// 设置基础信息
	void SetPosition(float x, float y, float w, float h, float r);

	// 通过逆时针顺序三个顶点设置矩形
	void SetPosition(Node n1, Node n2, Node n3);

	// 通过顺序无关四个顶点设置矩形
	void SetPosition(Node n1, Node n2, Node n3, Node n4);

protected:
	float centerX, centerY;
	float sizeX, sizeY;
	float rotation;

	float acreage;
};
