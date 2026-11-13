#include "product_burger.h"

using namespace std;

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
