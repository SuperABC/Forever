#pragma once

#include "util.h"

#include <vector>
#include <unordered_map>


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

class Rect {
public:
	Rect() : posX(0.f), posY(0.f), sizeX(0.f), sizeY(0.f) {}
	Rect(float x, float y, float w, float h) : posX(x), posY(y), sizeX(w), sizeY(h) {}

	// 获取/设置属性
	float GetPosX();
	void SetPosX(float x);
	float GetPosY();
	void SetPosY(float y);
	float GetSizeX();
	void SetSizeX(float w);
	float GetSizeY();
	void SetSizeY(float h);

protected:
	float posX, posY;
	float sizeX, sizeY;
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
	void AddRect(std::string name, std::shared_ptr<Rect> rect);
	std::shared_ptr<Rect> GetRect(std::string name);
	void RemoveRect(std::string name);

protected:
	float rotation;
	float acreage;

	std::vector<std::unordered_map<std::string, std::shared_ptr<Rect>>> rects;
};
