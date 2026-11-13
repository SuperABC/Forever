#include "asset_ammo_rifle.h"

using namespace std;

int RifleAmmoAsset::count = 0;

RifleAmmoAsset::RifleAmmoAsset() : id(count++) {
}

const char* RifleAmmoAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void RifleAmmoAsset::SetProperty() {
	mobility = AssetMobility::Containee;
	weight = 0.01f;
	size = 0.01f;
}
