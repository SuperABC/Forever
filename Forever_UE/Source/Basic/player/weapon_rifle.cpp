#include "weapon_rifle.h"

#include "common/handle.h"
#include "common/json.h"

using namespace std;

int RifleWeapon::count = 0;

RifleWeapon::RifleWeapon() : id(count++) {
}

const char* RifleWeapon::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void RifleWeapon::SetProperty() {
	fireMode = FireMode::Ballistic;
	damage = 40.f;
	fireRate = 0.1f;      // 每秒最多10发，全自动
	fullAuto = true;
	maxRange = 10000.f;    // 100米
	baseSpread = 0.f;      // 随机散射半角(度)，暂时设成0=没有散射
	muzzleVelocity = 40000.f; // 400m/s，100米处下坠约30cm
	magazineCapacity = 30;
	reloadDuration = 1.5f;

	firstPersonMeshPath = "/Game/3rdParty/Weapons_Free/Meshes/Actors/AC_rifle_001.AC_rifle_001_C";

	// 资源包的步枪模型约226cm长，缩放到约90cm。
	modelScale = 0.4f;

	// 瞄准锚点/背枪锚点(枪托)在Core给的基础位置上的微调(不填=基础位置)，见weapon_mod.h。
	// 只调这两个，不用anchorOffset。
	attachOffsetX = 0.f;
	attachOffsetZ = 0.f;
	backOffsetX = -5.f;
	backOffsetY = 4.f;
	backOffsetZ = 0.f;
	muzzleOffsetX = 44.f;
	muzzleOffsetZ = 11.f;

	// 后坐力比手枪重、范围更宽：全自动连发时准心明显上爬，需要玩家主动压枪。
	recoilPitchMin = 0.8f;
	recoilPitchMax = 1.5f;
	recoilYawMin = -0.8f;
	recoilYawMax = 0.8f;

	ammoType = "asset_ammo_rifle";
}
