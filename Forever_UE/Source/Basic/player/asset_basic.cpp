#include "asset_basic.h"

using namespace std;

int ContainerAsset::count = 0;

ContainerAsset::ContainerAsset() : id(count++) {
	mobility = AssetMobility::Object;
	weight = 2.0f;
	size = 5.0f;
	volume = 50.0f;
	backpack = true;
}

const char* ContainerAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

int WheatAsset::count = 0;

WheatAsset::WheatAsset() : id(count++) {
	mobility = AssetMobility::Containee;
	weight = 0.1f;
	size = 0.1f;
}

const char* WheatAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

int BeefAsset::count = 0;

BeefAsset::BeefAsset() : id(count++) {
	mobility = AssetMobility::Containee;
	weight = 0.1f;
	size = 0.1f;
}

const char* BeefAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

int BurgerAsset::count = 0;

BurgerAsset::BurgerAsset() : id(count++) {
	mobility = AssetMobility::Containee;
	weight = 0.2f;
	size = 0.2f;
	usable = true; // 可使用，用完count-1到0即销毁，不做饱食度效果
}

const char* BurgerAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

int PistolAmmoAsset::count = 0;

PistolAmmoAsset::PistolAmmoAsset() : id(count++) {
	mobility = AssetMobility::Containee;
	weight = 0.01f;
	size = 0.01f;
}

const char* PistolAmmoAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

int RifleAmmoAsset::count = 0;

RifleAmmoAsset::RifleAmmoAsset() : id(count++) {
	mobility = AssetMobility::Containee;
	weight = 0.01f;
	size = 0.01f;
}

const char* RifleAmmoAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

int PistolWeaponAsset::count = 0;

PistolWeaponAsset::PistolWeaponAsset() : id(count++) {
	mobility = AssetMobility::Object;
	weight = 3.0f;
	size = 8.0f;
	weapon = true; // 只能挂左肩/右肩，不能拿在手上
}

const char* PistolWeaponAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

int RifleWeaponAsset::count = 0;

RifleWeaponAsset::RifleWeaponAsset() : id(count++) {
	mobility = AssetMobility::Object;
	weight = 3.0f;
	size = 8.0f;
	weapon = true;
}

const char* RifleWeaponAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}
