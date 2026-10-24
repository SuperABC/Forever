#include "weapon_basic.h"

using namespace std;

int PistolWeapon::count = 0;

PistolWeapon::PistolWeapon() : id(count++) {
}

const char* PistolWeapon::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void PistolWeapon::SetProperty() {
	damage = 20.f;
	fireRate = 0.2f;     // 每秒最多5发
	fullAuto = false;
	maxRange = 5000.f;    // 50米
	baseSpread = 0.f;      // 随机散射半角(度)，暂时设成0=没有散射
	magazineCapacity = 8;
	reloadDuration = 2.f;

	// 武器模型：Actor Blueprint类(枪体+弹匣的相对位置在蓝图里)，路径以_C结尾，见
	// weapon_mod.h/ForeverWeaponVisualComponent.md。
	firstPersonMeshPath = "/Game/3rdParty/Weapons_Free/Meshes/Actors/AC_pistol_001.AC_pistol_001_C";

	// 资源包的手枪模型约97cm长(真实约20cm)，缩放到约24cm。
	modelScale = 0.25f;

	// 瞄准锚点/背枪锚点(枪托)在Core给的基础位置上的微调(不填=基础位置)，见weapon_mod.h。
	// 只调这两个，不用anchorOffset：手枪瞄准时更靠前+20(第一视角才看得到)，两处都再高1.5。
	attachOffsetX = 20.f;
	attachOffsetZ = 8.f;
	backOffsetX = -5.f;
	backOffsetY = -4.f;
	backOffsetZ = 10.f;
	muzzleOffsetX = 10.f;
	muzzleOffsetZ = 4.f;

	// 后坐力比步枪轻——手枪单手持握，仿PUBG手枪的后坐力手感，垂直方向按范围随机取值。
	recoilPitchMin = 1.2f;
	recoilPitchMax = 2.f;
	recoilYawMin = -1.2f;
	recoilYawMax = 1.2f;

	ammoType = "asset_ammo_pistol";
}

int RifleWeapon::count = 0;

RifleWeapon::RifleWeapon() : id(count++) {
}

const char* RifleWeapon::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void RifleWeapon::SetProperty() {
	damage = 40.f;
	fireRate = 0.1f;      // 每秒最多10发，全自动
	fullAuto = true;
	maxRange = 10000.f;    // 100米
	baseSpread = 0.f;      // 随机散射半角(度)，暂时设成0=没有散射
	magazineCapacity = 30;
	reloadDuration = 1.5f;

	firstPersonMeshPath = "/Game/3rdParty/Weapons_Free/Meshes/Actors/AC_rifle_001.AC_rifle_001_C";

	// 资源包的步枪模型约226cm长(真实约90cm)，缩放到约90cm。
	modelScale = 0.4f;

	// 瞄准锚点/背枪锚点(枪托)在Core给的基础位置上的微调(不填=基础位置)，见weapon_mod.h。
	// 只调这两个，不用anchorOffset：步枪两处都再低2。
	attachOffsetX = 0.f;
	attachOffsetZ = 0.f;
	backOffsetX = -5.f;
	backOffsetY = 4.f;
	backOffsetZ = 0.f;
	muzzleOffsetX = 44.f;
	muzzleOffsetZ = 11.f;

	// 后坐力比手枪重、且范围更宽——全自动连发时准心会明显往上爬、左右也漂得更开，
	// 仿PUBG步枪连发的手感，需要玩家主动压枪。
	recoilPitchMin = 0.8f;
	recoilPitchMax = 1.5f;
	recoilYawMin = -0.8f;
	recoilYawMax = 0.8f;

	ammoType = "asset_ammo_rifle";
}

int ShotgunWeapon::count = 0;

ShotgunWeapon::ShotgunWeapon() : id(count++) {
}

const char* ShotgunWeapon::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void ShotgunWeapon::SetProperty() {
	// 近距离高伤害、射速慢、泵动式(单发)。WeaponMod还没有"一次射出多颗弹丸"的字段，所以这里
	// 一次开火只有一条射线，用单发高伤害+小散射近似霰弹枪，数值留给PIE里试。
	damage = 90.f;
	fireRate = 0.9f;
	fullAuto = false;
	maxRange = 2500.f;     // 25米，射程短
	baseSpread = 2.5f;     // 随机散射半角(度)
	magazineCapacity = 6;
	reloadDuration = 2.5f;

	firstPersonMeshPath = "/Game/3rdParty/Weapons_Free/Meshes/Actors/AC_shotgun_001.AC_shotgun_001_C";

	// 3D变换先全部沿用步枪的数值，后面按模型实际大小再调。
	modelScale = 0.4f;
	attachOffsetX = 0.f;
	attachOffsetZ = 0.f;
	backOffsetX = -5.f;
	backOffsetY = 4.f;
	backOffsetZ = 0.f;
	muzzleOffsetX = 50.f;
	muzzleOffsetZ = 12.f;

	// 单发后坐力大，范围比步枪宽。
	recoilPitchMin = 3.f;
	recoilPitchMax = 4.5f;
	recoilYawMin = -1.5f;
	recoilYawMax = 1.5f;

	ammoType = "asset_ammo_shotgun";
}

int SniperWeapon::count = 0;

SniperWeapon::SniperWeapon() : id(count++) {
}

const char* SniperWeapon::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void SniperWeapon::SetProperty() {
	// 远距离高伤害、射速最慢、栓动式(单发)、不散射。
	damage = 150.f;
	fireRate = 1.5f;
	fullAuto = false;
	maxRange = 30000.f;    // 300米
	baseSpread = 0.f;
	magazineCapacity = 5;
	reloadDuration = 3.f;

	firstPersonMeshPath = "/Game/3rdParty/Weapons_Free/Meshes/Actors/AC_sniper_rifle_001.AC_sniper_rifle_001_C";

	// 3D变换先全部沿用步枪的数值，后面按模型实际大小再调。
	modelScale = 0.6f;
	attachOffsetX = 0.f;
	attachOffsetZ = 0.f;
	backOffsetX = -5.f;
	backOffsetY = 4.f;
	backOffsetZ = 0.f;
	muzzleOffsetX = 80.f;
	muzzleOffsetZ = 14.f;

	// 单发后坐力很大、左右漂得少。
	recoilPitchMin = 4.f;
	recoilPitchMax = 6.f;
	recoilYawMin = -0.8f;
	recoilYawMax = 0.8f;

	ammoType = "asset_ammo_sniper";
}
