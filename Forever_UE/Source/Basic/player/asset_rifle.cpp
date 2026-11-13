#include "asset_rifle.h"

using namespace std;

int RifleWeaponAsset::count = 0;

RifleWeaponAsset::RifleWeaponAsset() : id(count++) {
}

const char* RifleWeaponAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void RifleWeaponAsset::SetProperty() {
	mobility = AssetMobility::Object;
	weight = 3.0f;
	size = 8.0f;
	weapon = true;
	weaponType = "weapon_rifle";
	milestoneNames = { "asset_test" }; // 测试脚本：捡起/获得/扣除/丢弃选项，见Resource/Story/asset_test.script
}
