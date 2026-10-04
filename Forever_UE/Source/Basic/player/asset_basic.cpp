#include "asset_basic.h"

using namespace std;

int ContainerAsset::count = 0;

ContainerAsset::ContainerAsset() : id(count++) {
}

const char* ContainerAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void ContainerAsset::SetProperty() {
	mobility = AssetMobility::Object;
	weight = 2.0f;
	size = 5.0f;
	volume = 50.0f;
	backpack = true;
}

int WheatAsset::count = 0;

WheatAsset::WheatAsset() : id(count++) {
}

const char* WheatAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void WheatAsset::SetProperty() {
	mobility = AssetMobility::Containee;
	weight = 0.1f;
	size = 0.1f;
}

int BeefAsset::count = 0;

BeefAsset::BeefAsset() : id(count++) {
}

const char* BeefAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void BeefAsset::SetProperty() {
	mobility = AssetMobility::Containee;
	weight = 0.1f;
	size = 0.1f;
}

int BurgerAsset::count = 0;

BurgerAsset::BurgerAsset() : id(count++) {
}

const char* BurgerAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void BurgerAsset::SetProperty() {
	mobility = AssetMobility::Containee;
	weight = 0.2f;
	size = 0.2f;
	usable = true; // 可使用，用完count-1到0即销毁，不做饱食度效果
}

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
	weapon = true; // 只能挂左肩/右肩，不能拿在手上
}

int RifleWeaponAsset::count = 0;

RifleWeaponAsset::RifleWeaponAsset() : id(count++) {
}

const char* RifleWeaponAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void RifleWeaponAsset::SetProperty() {
	mobility = AssetMobility::Object;
	weight = 3.0f;
	size = 8.0f;
	weapon = true;
}
