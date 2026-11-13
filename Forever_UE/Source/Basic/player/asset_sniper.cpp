#include "asset_sniper.h"

using namespace std;

int SniperWeaponAsset::count = 0;

SniperWeaponAsset::SniperWeaponAsset() : id(count++) {
}

const char* SniperWeaponAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void SniperWeaponAsset::SetProperty() {
	mobility = AssetMobility::Object;
	weight = 4.5f;
	size = 10.0f;
	weapon = true;
	weaponType = "weapon_sniper";
	milestoneNames = { "asset_test" }; // 测试脚本：捡起/获得/扣除/丢弃选项，见Resource/Story/asset_test.script
}
