#include "product_basic.h"

using namespace std;

int WheatProduct::count = 0;

WheatProduct::WheatProduct() : id(count++) {
}

const char* WheatProduct::GetName() {
	name = "WheatProduct" + to_string(id);
	return name.data();
}

void WheatProduct::SetProperty() {
	categories = { "grain" };
	batchSize = 10.f; // 一批10份，无原料无副产品
}

int BeefProduct::count = 0;

BeefProduct::BeefProduct() : id(count++) {
}

const char* BeefProduct::GetName() {
	name = "BeefProduct" + to_string(id);
	return name.data();
}

void BeefProduct::SetProperty() {
	categories = { "meat" };
	batchSize = 10.f; // 一批10份，无原料无副产品
}

int BurgerProduct::count = 0;

BurgerProduct::BurgerProduct() : id(count++) {
}

const char* BurgerProduct::GetName() {
	name = "BurgerProduct" + to_string(id);
	return name.data();
}

void BurgerProduct::SetProperty() {
	categories = { "food" };
	batchSize = 5.f; // 一批5份，需要3份小麦+2份牛肉
	ingredients["product_wheat"] = 3.f;
	ingredients["product_beef"] = 2.f;
}
