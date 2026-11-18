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
	usable = 1;                   // 吃一次就用完一个(count-1，到0销毁)
	// 脚本：使用(use_asset)时加饱食度，见Resource/Story/asset_burger.script；使用一次count-1，到0销毁由UE层处理。
	milestoneNames = { "asset_burger" };
	productType = "product_burger"; // 商店按这个Product的零售价卖；使用一次count-1，到0销毁
}
