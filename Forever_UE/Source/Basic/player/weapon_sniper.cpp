#include "weapon_sniper.h"

#include "common/handle.h"
#include "common/json.h"

using namespace std;

int SniperWeapon::count = 0;

SniperWeapon::SniperWeapon() : id(count++) {
}

const char* SniperWeapon::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void SniperWeapon::SetProperty() {
	// 远距离高伤害、射速最慢、栓动式(单发)、不散射。
	fireMode = FireMode::Ballistic;
	damage = 150.f;
	fireRate = 1.5f;
	fullAuto = false;
	maxRange = 30000.f;    // 300米
	baseSpread = 0.f;
	muzzleVelocity = 70000.f; // 700m/s，300米处下坠约90cm
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
