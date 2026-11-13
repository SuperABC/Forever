#include "weapon_pistol.h"

#include "common/handle.h"
#include "common/json.h"

using namespace std;

int PistolWeapon::count = 0;

PistolWeapon::PistolWeapon() : id(count++) {
}

const char* PistolWeapon::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

void PistolWeapon::SetProperty() {
	fireMode = FireMode::Ballistic;
	damage = 20.f;
	fireRate = 0.2f;     // 每秒最多5发
	fullAuto = false;
	maxRange = 5000.f;    // 50米
	baseSpread = 0.f;      // 随机散射半角(度)，暂时设成0=没有散射
	muzzleVelocity = 25000.f; // 250m/s，游戏化初速(比真实枪低)，50米外下坠约20cm
	magazineCapacity = 8;
	reloadDuration = 2.f;

	// 武器模型：Actor Blueprint类，路径以_C结尾，见weapon_mod.h/ForeverWeaponVisualComponent.md。
	firstPersonMeshPath = "/Game/3rdParty/Weapons_Free/Meshes/Actors/AC_pistol_001.AC_pistol_001_C";

	// 资源包的手枪模型约97cm长，缩放到约24cm。
	modelScale = 0.25f;

	// 瞄准锚点/背枪锚点(枪托)在Core给的基础位置上的微调(不填=基础位置)，见weapon_mod.h。
	// 只调这两个，不用anchorOffset：手枪瞄准时更靠前+20(第一视角才看得到)。
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
