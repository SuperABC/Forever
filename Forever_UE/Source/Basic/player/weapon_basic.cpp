#include "weapon_basic.h"

using namespace std;

int PistolWeapon::count = 0;

PistolWeapon::PistolWeapon() : id(count++) {
	damage = 25.f;
	fireRate = 0.08f;     // 每秒最多12.5发，用户反馈之前0.2s(5发/秒)的射速太慢
	fullAuto = true;      // 按住左键连续开火——用户明确要求，弹匣也一并放大到30发
	maxRange = 4000.f;    // 40米
	baseSpread = 1.f;
	magazineCapacity = 30;
	reloadDuration = 1.5f;

	// 没有真实持枪socket，挂在角色Root上再做这个本地偏移——挪到右肩膀靠前一点的位置，
	// 不是身体正中心(实测卡在了胯部)。具体数值是目测给的，手感/位置需要在PIE里再调，
	// 见weapon_mod.h/ForeverWeaponComponent.md。
	attachOffsetX = 20.f;
	attachOffsetY = 20.f;
	attachOffsetZ = 40.f;

	// 后坐力比步枪轻——手枪单手持握，仿PUBG手枪的后坐力手感。垂直方向也改成范围随机
	// (之前是固定值1.0)，两个方向手感一致。
	recoilPitchMin = 0.8f;
	recoilPitchMax = 1.2f;
	recoilYawMin = -0.6f;
	recoilYawMax = 0.6f;

	ammoType = "ammo_pistol";
}

const char* PistolWeapon::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}

int RifleWeapon::count = 0;

RifleWeapon::RifleWeapon() : id(count++) {
	damage = 18.f;
	fireRate = 0.1f;      // 每秒最多10发，全自动
	fullAuto = true;
	maxRange = 8000.f;    // 80米
	baseSpread = 2.5f;    // 全自动散射更大
	magazineCapacity = 30;
	reloadDuration = 2.2f;

	attachOffsetX = 20.f;
	attachOffsetY = 20.f;
	attachOffsetZ = 40.f;

	// 后坐力比手枪重、且范围更宽——全自动连发时准心会明显往上爬、左右也漂得更开，
	// 仿PUBG步枪连发的手感，需要玩家主动压枪。
	recoilPitchMin = 0.45f;
	recoilPitchMax = 0.75f;
	recoilYawMin = -0.4f;
	recoilYawMax = 0.4f;

	ammoType = "ammo_rifle";
}

const char* RifleWeapon::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}
