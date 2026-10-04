#pragma once

#include "player/weapon_mod.h"
#include "player/weapon_factory.h"

#include <string>


// Weapon：一把武器的实例——照抄Asset的"factory+mod，构造时把字段原样拷一份到自己身上"
// 模式(Pattern B，见asset.h/asset.md)，不把WeaponMod*直接暴露给Forever层，所有字段都
// 通过Getter读。构造时调一次mod->SetProperty()(两段式约定，和Asset::Asset()同一个
// 调用时机)，不依赖调用方(原来是Forever层的EquipWeapon())记得调，见weapon_mod.h。
//
// 这个类补的是一个纯粹的历史遗留空档——WeaponMod/WeaponFactory落地时(weapon_system_plan.md)
// 的文件清单压根没规划Core层wrapper，此后一直是Forever层的UForeverWeaponComponent直接
// 拿裸WeaponMod*用，和其余所有concept(Asset/Room/Storage/...)都有Core wrapper的既定
// 结构不一致，这次补上，不是改行为。
class Weapon {
public:
	// id需要在config.json的"weapon_mods"里启用，否则mod为空、IsValid()返回false。
	Weapon(WeaponFactory* factory, const std::string& id);
	~Weapon();

	bool IsValid() const;

	const std::string& GetType() const;

	const std::string& GetFirstPersonMeshPath() const;
	const std::string& GetGripSocketName() const;
	float GetAttachOffsetX() const;
	float GetAttachOffsetY() const;
	float GetAttachOffsetZ() const;

	float GetDamage() const;
	float GetFireRate() const;
	bool IsFullAuto() const;
	float GetMaxRange() const;
	float GetBaseSpread() const;

	int GetMagazineCapacity() const;
	float GetReloadDuration() const;
	const std::string& GetAmmoType() const;

	float GetRecoilPitchMin() const;
	float GetRecoilPitchMax() const;
	float GetRecoilYawMin() const;
	float GetRecoilYawMax() const;

	// 转发给mod->ComputeDamage(distance)——保留mod指针活到Weapon析构，就是为了这个转发
	// (唯一一个没法在构造时就拷完值的字段，因为它是按距离算的，不是常量)。
	float ComputeDamage(float distance) const;

private:
	WeaponFactory* factory;
	WeaponMod* mod;
	std::string type;

	std::string firstPersonMeshPath;
	std::string gripSocketName;
	float attachOffsetX = 0.f;
	float attachOffsetY = 0.f;
	float attachOffsetZ = 0.f;

	float damage = 0.f;
	float fireRate = 0.f;
	bool fullAuto = false;
	float maxRange = 0.f;
	float baseSpread = 0.f;

	int magazineCapacity = 0;
	float reloadDuration = 0.f;
	std::string ammoType;

	float recoilPitchMin = 0.f;
	float recoilPitchMax = 0.f;
	float recoilYawMin = 0.f;
	float recoilYawMax = 0.f;
};
