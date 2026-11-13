#include "asset_shotgun.h"

using namespace std;

int ShotgunWeaponAsset::count = 0;

ShotgunWeaponAsset::ShotgunWeaponAsset() : id(count++) {
}

const char* ShotgunWeaponAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void ShotgunWeaponAsset::SetProperty() {
	mobility = AssetMobility::Object;
	weight = 3.5f;
	size = 9.0f;
	weapon = true;
	weaponType = "weapon_shotgun";
	milestoneNames = { "asset_test" }; // 测试脚本：捡起/获得/扣除/丢弃选项，见Resource/Story/asset_test.script
}
