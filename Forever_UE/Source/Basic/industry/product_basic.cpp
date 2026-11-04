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
	// 价格(分)：零售8元>进口6元>出口4元。
	retailPrice = 800;
	importPrice = 600;
	exportPrice = 400;
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
	retailPrice = 1600; // 零售16元>进口12元>出口8元
	importPrice = 1200;
	exportPrice = 800;
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
	retailPrice = 5000; // 零售50元>进口30元>出口20元
	importPrice = 3000;
	exportPrice = 2000;
	// 纯预制菜，质量只有0.1；小麦/牛肉暂不开放市民直接购买，serviceType留空。
	serviceType = "food";
	serviceQuality = 0.1f;
}
