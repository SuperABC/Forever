#include "weapon_shotgun.h"

#include "common/handle.h"
#include "common/json.h"

using namespace std;

int ShotgunWeapon::count = 0;

ShotgunWeapon::ShotgunWeapon() : id(count++) {
}

const char* ShotgunWeapon::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void ShotgunWeapon::SetProperty() {
	// 近距离高伤害、射速慢、泵动式(单发)。一次开火射出pelletCount颗弹丸，每颗独立散射、独立结算伤害，
	// 所以damage是单颗弹丸的伤害(8颗全中约120)。
	fireMode = FireMode::Ballistic;
	damage = 15.f;
	fireRate = 0.9f;
	fullAuto = false;
	maxRange = 2500.f;     // 25米，射程短
	baseSpread = 2.5f;     // 随机散射半角(度)
	muzzleVelocity = 20000.f; // 200m/s
	pelletCount = 8;
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
