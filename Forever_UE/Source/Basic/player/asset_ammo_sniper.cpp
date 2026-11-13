#include "asset_ammo_sniper.h"

using namespace std;

int SniperAmmoAsset::count = 0;

SniperAmmoAsset::SniperAmmoAsset() : id(count++) {
}

const char* SniperAmmoAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void SniperAmmoAsset::SetProperty() {
	mobility = AssetMobility::Containee;
	weight = 0.015f;
	size = 0.015f;
}
