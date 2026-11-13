#include "manufacture_ranch.h"

using namespace std;

int RanchManufacture::count = 0;

RanchManufacture::RanchManufacture() : id(count++) {
}

const char* RanchManufacture::GetName() {
	name = "RanchManufacture" + to_string(id);
	return name.data();
}

void RanchManufacture::SetTargets() {
	targets["product_beef"] = 20; // 每天20批(每批10份)=200份牛肉，无原料
}
