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
	weaponType = "weapon_pistol";
	milestoneNames = { "asset_test" }; // 测试脚本：捡起/获得/扣除/丢弃选项，见Resource/Story/asset_test.script
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
	weaponType = "weapon_rifle";
	milestoneNames = { "asset_test" }; // 测试脚本：捡起/获得/扣除/丢弃选项，见Resource/Story/asset_test.script
}

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

int SniperWeaponAsset::count = 0;

SniperWeaponAsset::SniperWeaponAsset() : id(count++) {
}

const char* SniperWeaponAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void SniperWeaponAsset::SetProperty() {
	mobility = AssetMobility::Object;
	weight = 4.5f;
	size = 10.0f;
	weapon = true;
	weaponType = "weapon_sniper";
	milestoneNames = { "asset_test" }; // 测试脚本：捡起/获得/扣除/丢弃选项，见Resource/Story/asset_test.script
}

int GrenadeAsset::count = 0;

GrenadeAsset::GrenadeAsset() : id(count++) {
}

const char* GrenadeAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void GrenadeAsset::SetProperty() {
	mobility = AssetMobility::Object;
	weight = 0.4f;
	size = 0.3f;
	throwable = true;
	meshPath = "/Game/Asset/Meshes/Sphere.Sphere";
	meshScale = 0.1f; // 100cm直径的球体缩到10cm
}

int SmokeGrenadeAsset::count = 0;

SmokeGrenadeAsset::SmokeGrenadeAsset() : id(count++) {
}

const char* SmokeGrenadeAsset::GetName() {
	name = string(GetType()) + to_string(id);
	return name.data();
}

void SmokeGrenadeAsset::SetProperty() {
	mobility = AssetMobility::Object;
	weight = 0.4f;
	size = 0.3f;
	throwable = true;
	meshPath = "/Game/Asset/Meshes/Sphere.Sphere";
	meshScale = 0.1f;
}
