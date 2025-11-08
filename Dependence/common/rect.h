#pragma once

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
