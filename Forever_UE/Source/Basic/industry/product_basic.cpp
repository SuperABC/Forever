#include "product_basic.h"

using namespace std;

int WheatProduct::count = 0;

WheatProduct::WheatProduct() : id(count++) {
	categories = { "grain" };
	batchSize = 10.f; // 一批10份，无原料无副产品
}

const char* WheatProduct::GetName() {
	name = "WheatProduct" + to_string(id);
	return name.data();
}

int BeefProduct::count = 0;

BeefProduct::BeefProduct() : id(count++) {
	categories = { "meat" };
	batchSize = 10.f; // 一批10份，无原料无副产品
}

const char* BeefProduct::GetName() {
	name = "BeefProduct" + to_string(id);
	return name.data();
}

int BurgerProduct::count = 0;

BurgerProduct::BurgerProduct() : id(count++) {
	categories = { "food" };
	batchSize = 5.f; // 一批5份，需要3份小麦+2份牛肉
	ingredients["product_wheat"] = 3.f;
	ingredients["product_beef"] = 2.f;
}

const char* BurgerProduct::GetName() {
	name = "BurgerProduct" + to_string(id);
	return name.data();
}
