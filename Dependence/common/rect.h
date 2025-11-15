#pragma once

class Rect {
public:
	Rect();
	Rect(float x, float y, float w, float h);
	virtual ~Rect() = default;

	// 获取/设置属性
	float GetPosX() const;
	void SetPosX(float x);
	float GetPosY() const;
	void SetPosY(float y);
	float GetSizeX() const;
	void SetSizeX(float w);
	float GetSizeY() const;
	void SetSizeY(float h);
	float GetLeft() const;
	float GetRight() const;
	float GetBottom() const;
	float GetTop() const;
	void SetVertices(float x1, float y1, float x2, float y2);
	void SetPosition(float x, float y, float w, float h);
	float GetAcreage() const;
	void SetAcreage(float a);

protected:
	float posX, posY;
	float sizeX, sizeY;

	float acreage;
};
