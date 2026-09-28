#include "manufacture_basic.h"

using namespace std;

int FarmManufacture::count = 0;

FarmManufacture::FarmManufacture() : id(count++) {
}

const char* FarmManufacture::GetName() {
	name = "FarmManufacture" + to_string(id);
	return name.data();
}

void FarmManufacture::SetTargets() {
	targets["product_wheat"] = 1; // 每天1批(10份)小麦，无原料
}

int RanchManufacture::count = 0;

RanchManufacture::RanchManufacture() : id(count++) {
}

const char* RanchManufacture::GetName() {
	name = "RanchManufacture" + to_string(id);
	return name.data();
}

void RanchManufacture::SetTargets() {
	targets["product_beef"] = 1; // 每天1批(10份)牛肉，无原料
}

int FoodFactoryManufacture::count = 0;

FoodFactoryManufacture::FoodFactoryManufacture() : id(count++) {
}

const char* FoodFactoryManufacture::GetName() {
	name = "FoodFactoryManufacture" + to_string(id);
	return name.data();
}

void FoodFactoryManufacture::SetTargets() {
	targets["product_burger"] = 1; // 每天1批(5份)汉堡，需要3小麦+2牛肉(配方见BurgerProduct)
}
