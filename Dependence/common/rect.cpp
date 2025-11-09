#include "rect.h"

#include <algorithm>


using namespace std;

float Rect::GetPosX() const {
    return posX;
}

void Rect::SetPosX(float x) {
    posX = x;
}

float Rect::GetPosY() const {
    return posY;
}

void Rect::SetPosY(float y) {
    posY = y;
}

float Rect::GetSizeX() const {
    return sizeX;
}

void Rect::SetSizeX(float w) {
    sizeX = w;
}

float Rect::GetSizeY() const {
    return sizeY;
}

void Rect::SetSizeY(float h) {
    sizeY = h;
}

float Rect::GetLeft() const {
    return posX - sizeX / 2.f;
}

float Rect::GetRight() const {
    return posX + sizeX / 2.f;
}

float Rect::GetBottom() const {
    return posY - sizeY / 2.f;
}

float Rect::GetTop() const {
    return posY + sizeY / 2.f;
}

void Rect::SetVertices(float x1, float y1, float x2, float y2) {
    if (x1 > x2) {
        swap(x1, x2);
    }
    if (y1 > y2) {
        swap(y1, y2);
    }

    posX = (x1 + x2) / 2.f;
    posY = (y1 + y2) / 2.f;
    sizeX = x2 - x1;
    sizeY = y2 - y1;
}

void Rect::SetPosition(float x, float y, float w, float h) {
    posX = x;
    posY = y;
    sizeX = w;
    sizeY = h;
}

float Rect::GetAcreage() const {
    return acreage;
}

void Rect::SetAcreage(float a) {
    acreage = a;
}
