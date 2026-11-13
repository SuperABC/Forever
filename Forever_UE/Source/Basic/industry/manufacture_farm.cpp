#include "manufacture_farm.h"

using namespace std;

int FarmManufacture::count = 0;

FarmManufacture::FarmManufacture() : id(count++) {
}

const char* FarmManufacture::GetName() {
	name = "FarmManufacture" + to_string(id);
	return name.data();
}

void FarmManufacture::SetTargets() {
	targets["product_wheat"] = 20; // 每天20批(每批10份)=200份小麦，无原料
}
