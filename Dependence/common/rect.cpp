#include "rect.h"


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

float Rect::GetAcreage() const {
    return acreage;
}

void Rect::SetAcreage(float a) {
    acreage = a;
}
