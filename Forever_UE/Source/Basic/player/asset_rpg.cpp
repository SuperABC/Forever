#include "asset_rpg.h"

using namespace std;

int RpgWeaponAsset::count = 0;

RpgWeaponAsset::RpgWeaponAsset() : id(count++) {
}

const char* RpgWeaponAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void RpgWeaponAsset::SetProperty() {
	mobility = AssetMobility::Object;
	weight = 7.0f;
	size = 12.0f;
	weapon = true;
	weaponType = "weapon_rpg";
	milestoneNames = { "asset_test" }; // 测试脚本：捡起/获得/扣除/丢弃选项，见Resource/Story/asset_test.script
}
