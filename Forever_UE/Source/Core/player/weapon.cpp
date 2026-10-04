#include "player/weapon.h"

using namespace std;

Weapon::Weapon(WeaponFactory* factory, const string& id) :
	factory(factory), mod(nullptr) {
	mod = factory->CreateWeapon(id);
	if (!mod) return;

	mod->SetProperty();
	type = mod->GetType();
	firstPersonMeshPath = mod->firstPersonMeshPath;
	gripSocketName = mod->gripSocketName;
	attachOffsetX = mod->attachOffsetX;
	attachOffsetY = mod->attachOffsetY;
	attachOffsetZ = mod->attachOffsetZ;
	damage = mod->damage;
	fireRate = mod->fireRate;
	fullAuto = mod->fullAuto;
	maxRange = mod->maxRange;
	baseSpread = mod->baseSpread;
	magazineCapacity = mod->magazineCapacity;
	reloadDuration = mod->reloadDuration;
	ammoType = mod->ammoType;
	recoilPitchMin = mod->recoilPitchMin;
	recoilPitchMax = mod->recoilPitchMax;
	recoilYawMin = mod->recoilYawMin;
	recoilYawMax = mod->recoilYawMax;
}

Weapon::~Weapon() {
	if (mod) factory->DestroyWeapon(mod);
}

bool Weapon::IsValid() const { return mod != nullptr; }

const string& Weapon::GetType() const { return type; }

const string& Weapon::GetFirstPersonMeshPath() const { return firstPersonMeshPath; }
const string& Weapon::GetGripSocketName() const { return gripSocketName; }
float Weapon::GetAttachOffsetX() const { return attachOffsetX; }
float Weapon::GetAttachOffsetY() const { return attachOffsetY; }
float Weapon::GetAttachOffsetZ() const { return attachOffsetZ; }

float Weapon::GetDamage() const { return damage; }
float Weapon::GetFireRate() const { return fireRate; }
bool Weapon::IsFullAuto() const { return fullAuto; }
float Weapon::GetMaxRange() const { return maxRange; }
float Weapon::GetBaseSpread() const { return baseSpread; }

int Weapon::GetMagazineCapacity() const { return magazineCapacity; }
float Weapon::GetReloadDuration() const { return reloadDuration; }
const string& Weapon::GetAmmoType() const { return ammoType; }

float Weapon::GetRecoilPitchMin() const { return recoilPitchMin; }
float Weapon::GetRecoilPitchMax() const { return recoilPitchMax; }
float Weapon::GetRecoilYawMin() const { return recoilYawMin; }
float Weapon::GetRecoilYawMax() const { return recoilYawMax; }

float Weapon::ComputeDamage(float distance) const {
	return mod ? mod->ComputeDamage(distance) : 0.f;
}
