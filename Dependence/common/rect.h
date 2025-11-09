#pragma once

class Rect {
public:
	Rect() : posX(0.f), posY(0.f), sizeX(0.f), sizeY(0.f), acreage(0.f) {}
	Rect(float x, float y, float w, float h) : posX(x), posY(y), sizeX(w), sizeY(h), acreage(w * h * 100.f) {}

	// 获取/设置属性
	float GetPosX() const;
	void SetPosX(float x);
	float GetPosY() const;
	void SetPosY(float y);
	float GetSizeX() const;
	void SetSizeX(float w);
	float GetSizeY() const;
	void SetSizeY(float h);
	float GetAcreage() const;
	void SetAcreage(float a);

protected:
	float posX, posY;
	float sizeX, sizeY;

	float acreage;
};
