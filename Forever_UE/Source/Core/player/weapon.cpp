#include "player/weapon.h"

using namespace std;

namespace {
	// 瞄准锚点/背枪锚点(枪托)的基础位置由Core决定，角色本地，cm，X前/Y右/Z上：
	// - 瞄准：相对胶囊根，右手靠前一点；
	// - 背枪：相对背枪骨骼(组件的holsterBone)，右肩肩胛附近，左肩的枪在UE侧自动取反Y。
	// WeaponMod的attachOffset/backOffset只是在这个基础位置上的微调，不填(0)就是基础位置。
	constexpr float kAimBaseX = 20.f;
	constexpr float kAimBaseY = 20.f;
	constexpr float kAimBaseZ = 40.f;
	constexpr float kBackBaseX = -22.f;
	constexpr float kBackBaseY = 16.f;
	constexpr float kBackBaseZ = 12.f;
}

Weapon::Weapon(WeaponFactory* factory, const string& id) :
	factory(factory), mod(nullptr) {
	mod = factory->CreateWeapon(id);
	if (!mod) return;

	mod->SetProperty();
	type = mod->GetType();
	firstPersonMeshPath = mod->firstPersonMeshPath;
	gripSocketName = mod->gripSocketName;
	// attachOffset/backOffset存的是"基础位置+mod微调"之后的最终位置，Getter读到的就是最终位置。
	attachOffsetX = kAimBaseX + mod->attachOffsetX;
	attachOffsetY = kAimBaseY + mod->attachOffsetY;
	attachOffsetZ = kAimBaseZ + mod->attachOffsetZ;
	modelScale = mod->modelScale;
	anchorOffsetX = mod->anchorOffsetX;
	anchorOffsetY = mod->anchorOffsetY;
	anchorOffsetZ = mod->anchorOffsetZ;
	muzzleOffsetX = mod->muzzleOffsetX;
	muzzleOffsetY = mod->muzzleOffsetY;
	muzzleOffsetZ = mod->muzzleOffsetZ;
	backOffsetX = kBackBaseX + mod->backOffsetX;
	backOffsetY = kBackBaseY + mod->backOffsetY;
	backOffsetZ = kBackBaseZ + mod->backOffsetZ;
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
float Weapon::GetModelScale() const { return modelScale; }
float Weapon::GetAnchorOffsetX() const { return anchorOffsetX; }
float Weapon::GetAnchorOffsetY() const { return anchorOffsetY; }
float Weapon::GetAnchorOffsetZ() const { return anchorOffsetZ; }
float Weapon::GetMuzzleOffsetX() const { return muzzleOffsetX; }
float Weapon::GetMuzzleOffsetY() const { return muzzleOffsetY; }
float Weapon::GetMuzzleOffsetZ() const { return muzzleOffsetZ; }
float Weapon::GetBackOffsetX() const { return backOffsetX; }
float Weapon::GetBackOffsetY() const { return backOffsetY; }
float Weapon::GetBackOffsetZ() const { return backOffsetZ; }

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
