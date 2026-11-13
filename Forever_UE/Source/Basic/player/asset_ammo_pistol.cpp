#include "asset_ammo_pistol.h"

using namespace std;

int PistolAmmoAsset::count = 0;

PistolAmmoAsset::PistolAmmoAsset() : id(count++) {
}

const char* PistolAmmoAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void PistolAmmoAsset::SetProperty() {
	mobility = AssetMobility::Containee;
	weight = 0.01f;
	size = 0.01f;
}
