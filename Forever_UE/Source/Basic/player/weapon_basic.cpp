#include "weapon_basic.h"

using namespace std;

int PistolWeapon::count = 0;

PistolWeapon::PistolWeapon() : id(count++) {
	damage = 25.f;
	fireRate = 0.2f;      // 每秒最多5发，半自动手感
	fullAuto = false;
	maxRange = 4000.f;    // 40米
	baseSpread = 1.f;
	magazineCapacity = 12;
	reloadDuration = 1.5f;
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
}

const char* RifleWeapon::GetName() {
	name = string(GetType()) + std::to_string(id);
	return name.data();
}
