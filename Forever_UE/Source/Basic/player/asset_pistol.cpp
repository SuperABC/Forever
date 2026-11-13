#include "asset_pistol.h"

using namespace std;

int PistolWeaponAsset::count = 0;

PistolWeaponAsset::PistolWeaponAsset() : id(count++) {
}

const char* PistolWeaponAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void PistolWeaponAsset::SetProperty() {
	mobility = AssetMobility::Object;
	weight = 3.0f;
	size = 8.0f;
	weapon = true; // 能挂左肩/右肩；左右手拒绝武器(见AssetMod::weapon)
	weaponType = "weapon_pistol";
	milestoneNames = { "asset_test" }; // 测试脚本：捡起/获得/扣除/丢弃选项，见Resource/Story/asset_test.script
}
