#include "asset_ammo_rpg.h"

using namespace std;

int RpgAmmoAsset::count = 0;

RpgAmmoAsset::RpgAmmoAsset() : id(count++) {
}

const char* RpgAmmoAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void RpgAmmoAsset::SetProperty() {
	mobility = AssetMobility::Containee;
	weight = 2.0f;
	size = 1.0f;
}
