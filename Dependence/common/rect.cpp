#include "rect.h"


using namespace std;

float Rect::GetPosX() {
    return posX;
}

void Rect::SetPosX(float x) {
    posX = x;
}

float Rect::GetPosY() {
    return posY;
}

void Rect::SetPosY(float y) {
    posY = y;
}

float Rect::GetSizeX() {
    return sizeX;
}

void Rect::SetSizeX(float w) {
    sizeX = w;
}

float Rect::GetSizeY() {
    return sizeY;
}

void Rect::SetSizeY(float h) {
    sizeY = h;
}
