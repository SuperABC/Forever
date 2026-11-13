#include "asset_burger.h"

using namespace std;

int BurgerAsset::count = 0;

BurgerAsset::BurgerAsset() : id(count++) {
}

const char* BurgerAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void BurgerAsset::SetProperty() {
	mobility = AssetMobility::Object;   // Object才能拿在手上：没背包时商店按空手数限制购买量
	weight = 0.2f;
	size = 0.2f;
	usable = true;
	satiety = 0.5f;               // 吃一个汉堡饱食度+0.5(上限1，超出截断)
	productType = "product_burger"; // 商店按这个Product的零售价卖；使用一次count-1，到0销毁
}
