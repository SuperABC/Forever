#include "manufacture_food.h"

using namespace std;

int FoodFactoryManufacture::count = 0;

FoodFactoryManufacture::FoodFactoryManufacture() : id(count++) {
}

const char* FoodFactoryManufacture::GetName() {
	name = "FoodFactoryManufacture" + to_string(id);
	return name.data();
}

void FoodFactoryManufacture::SetTargets() {
	targets["product_burger"] = 100; // 每天100批(每批5份)=500份汉堡，每批需要3小麦+2牛肉(配方见BurgerProduct)
}
