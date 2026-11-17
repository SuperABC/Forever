#pragma once

#include "player/weapon_mod.h"
#include "player/weapon_factory.h"

#include <string>

class Script;


// Weapon：一把武器的实例——和Asset一致的"factory+mod，构造时把字段原样拷一份到自己身上"
// 模式(见asset.h/asset.md)，不把WeaponMod*直接暴露给Forever层，所有字段都通过Getter读。
// 构造时调一次mod->SetProperty()(两段式约定，和Asset::Asset()同一个调用时机)，不依赖
// 调用方记得调，见weapon_mod.h。
//
// 和其余所有concept(Asset/Room/Storage/...)一致：Dependence mod -> Core wrapper ->
// Basic默认内容三层结构，Weapon是这里的Core层wrapper。
class Weapon {
public:
	// id需要在config.json的"weapon_mods"里启用，否则mod为空、IsValid()返回false。
	Weapon(WeaponFactory* factory, const std::string& id);
	~Weapon();

	bool IsValid() const;

	const std::string& GetType() const;

	// 武器脚本(mod的scriptModName/milestoneNames，都空则为nullptr，见MakeScript)：开火事件(weapon_fire)发给它，
	// 开火后的特效和伤害都由它调用Change完成。脚本和武器解耦(不依赖肩上的Asset)，Weapon析构时一起销毁，不进存档。
	// 构造Weapon时不创建(FindDef这类只读模型参数的临时Weapon不需要)，装备武器的地方调一次EnsureScript。
	// 稳定键"weapon:<武器id>"，同一时刻只装备一把，create_timer等按键查找脚本靠这个键。
	void EnsureScript();
	Script* GetScript() const;

	const std::string& GetFirstPersonMeshPath() const;
	const std::string& GetGripSocketName() const;
	float GetAttachOffsetX() const;
	float GetAttachOffsetY() const;
	float GetAttachOffsetZ() const;

	float GetModelScale() const;
	float GetAnchorOffsetX() const;
	float GetAnchorOffsetY() const;
	float GetAnchorOffsetZ() const;
	float GetMuzzleOffsetX() const;
	float GetMuzzleOffsetY() const;
	float GetMuzzleOffsetZ() const;
	float GetBackOffsetX() const;
	float GetBackOffsetY() const;
	float GetBackOffsetZ() const;

	FireMode GetFireMode() const;
	float GetFireRate() const;
	bool IsFullAuto() const;
	float GetMaxRange() const;
	float GetBaseSpread() const;
	int GetPelletCount() const;

	int GetMagazineCapacity() const;
	// 弹匣容量大于0才用弹药；相机这类武器为false(不扣子弹、不换弹)。
	bool UsesAmmo() const;
	float GetReloadDuration() const;
	const std::string& GetAmmoType() const;

	float GetRecoilPitchMin() const;
	float GetRecoilPitchMax() const;
	float GetRecoilYawMin() const;
	float GetRecoilYawMax() const;

	// fireMode==Custom的武器开火：转发给mod->OnFire(post)，由mod自己通过post向Core发请求(保留mod指针活到Weapon析构就是为了这个转发)。
	void FireCustom(PostHandle* post) const;

private:
	WeaponFactory* factory;
	WeaponMod* mod;
	std::string type;
	Script* script = nullptr;

	std::string firstPersonMeshPath;
	std::string gripSocketName;
	float attachOffsetX = 0.f;
	float attachOffsetY = 0.f;
	float attachOffsetZ = 0.f;

	float modelScale = 1.f;
	float anchorOffsetX = 0.f;
	float anchorOffsetY = 0.f;
	float anchorOffsetZ = 0.f;
	float muzzleOffsetX = 0.f;
	float muzzleOffsetY = 0.f;
	float muzzleOffsetZ = 0.f;
	float backOffsetX = 0.f;
	float backOffsetY = 0.f;
	float backOffsetZ = 0.f;

	FireMode fireMode = FireMode::Hitscan;
	float fireRate = 0.f;
	bool fullAuto = false;
	float maxRange = 0.f;
	float baseSpread = 0.f;
	int pelletCount = 1;

	int magazineCapacity = 0;
	float reloadDuration = 0.f;
	std::string ammoType;

	float recoilPitchMin = 0.f;
	float recoilPitchMax = 0.f;
	float recoilYawMin = 0.f;
	float recoilYawMax = 0.f;
};
