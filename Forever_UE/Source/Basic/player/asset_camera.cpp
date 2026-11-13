#include "asset_camera.h"

using namespace std;

int CameraWeaponAsset::count = 0;

CameraWeaponAsset::CameraWeaponAsset() : id(count++) {
}

const char* CameraWeaponAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void CameraWeaponAsset::SetProperty() {
	mobility = AssetMobility::Object;
	weight = 0.8f;
	size = 3.0f;
	weapon = true;
	weaponType = "weapon_camera";
}
