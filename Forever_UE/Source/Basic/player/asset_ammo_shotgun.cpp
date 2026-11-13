#include "asset_ammo_shotgun.h"

using namespace std;

int ShotgunAmmoAsset::count = 0;

ShotgunAmmoAsset::ShotgunAmmoAsset() : id(count++) {
}

const char* ShotgunAmmoAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void ShotgunAmmoAsset::SetProperty() {
	mobility = AssetMobility::Containee;
	weight = 0.02f;
	size = 0.02f;
}
