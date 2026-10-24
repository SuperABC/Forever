#pragma once

#include "player/asset_mod.h"

#include <string>


// 物品/背包系统测试案例——8个具体资产：
// - ContainerAsset：背包容器(cube占位)，能背在后背。
// - WheatAsset/BeefAsset/BurgerAsset：id和Industry的WheatProduct/BeefProduct/
//   BurgerProduct共用(见Source/Basic/industry/product_basic.h)，Asset只存这个type字符串，
//   不持有Product实例，见Source/Core/player/asset.md"武器/背包系统的桥接"一节。汉堡可使用
//   (usable=true)，用完count-1到0即销毁，不做饱食度效果。
// - PistolAmmoAsset/RifleAmmoAsset/ShotgunAmmoAsset/SniperAmmoAsset：子弹，新id，没有对应Product。
// - PistolWeaponAsset/RifleWeaponAsset/ShotgunWeaponAsset/SniperWeaponAsset：id和WeaponMod的
//   PistolWeapon/RifleWeapon/ShotgunWeapon/SniperWeapon共用，
//   weapon=true——可以挂左肩/右肩(激活只走肩膀)，也可以拿在手上，见Source/Core/player/player.h的
//   ActivateShoulderWeapon桥接机制。
//
// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id+GetName里现拼。

class ContainerAsset : public AssetMod {
public:
	ContainerAsset();

	static const char* GetId() { return "asset_container"; }
	virtual const char* GetType() const override { return "asset_container"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class WheatAsset : public AssetMod {
public:
	WheatAsset();

	static const char* GetId() { return "asset_wheat"; }
	virtual const char* GetType() const override { return "asset_wheat"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class BeefAsset : public AssetMod {
public:
	BeefAsset();

	static const char* GetId() { return "asset_beef"; }
	virtual const char* GetType() const override { return "asset_beef"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class BurgerAsset : public AssetMod {
public:
	BurgerAsset();

	static const char* GetId() { return "asset_burger"; }
	virtual const char* GetType() const override { return "asset_burger"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class PistolAmmoAsset : public AssetMod {
public:
	PistolAmmoAsset();

	static const char* GetId() { return "asset_ammo_pistol"; }
	virtual const char* GetType() const override { return "asset_ammo_pistol"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class RifleAmmoAsset : public AssetMod {
public:
	RifleAmmoAsset();

	static const char* GetId() { return "asset_ammo_rifle"; }
	virtual const char* GetType() const override { return "asset_ammo_rifle"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class PistolWeaponAsset : public AssetMod {
public:
	PistolWeaponAsset();

	static const char* GetId() { return "asset_pistol"; }
	virtual const char* GetType() const override { return "asset_pistol"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class RifleWeaponAsset : public AssetMod {
public:
	RifleWeaponAsset();

	static const char* GetId() { return "asset_rifle"; }
	virtual const char* GetType() const override { return "asset_rifle"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class ShotgunAmmoAsset : public AssetMod {
public:
	ShotgunAmmoAsset();

	static const char* GetId() { return "asset_ammo_shotgun"; }
	virtual const char* GetType() const override { return "asset_ammo_shotgun"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class SniperAmmoAsset : public AssetMod {
public:
	SniperAmmoAsset();

	static const char* GetId() { return "asset_ammo_sniper"; }
	virtual const char* GetType() const override { return "asset_ammo_sniper"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class ShotgunWeaponAsset : public AssetMod {
public:
	ShotgunWeaponAsset();

	static const char* GetId() { return "asset_shotgun"; }
	virtual const char* GetType() const override { return "asset_shotgun"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class SniperWeaponAsset : public AssetMod {
public:
	SniperWeaponAsset();

	static const char* GetId() { return "asset_sniper"; }
	virtual const char* GetType() const override { return "asset_sniper"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

// 投掷物：手雷/烟雾弹。throwable=true，Object机动性(能拿在手上、放进容器，不能挂肩膀)。
// 网格用项目自己路径下的球体(Content/Asset/Meshes/Sphere)，直径100cm，meshScale缩到10cm。
// 落地后的效果(爆炸/烟雾)本期没做，只是被扔出去、5秒后消失。
class GrenadeAsset : public AssetMod {
public:
	GrenadeAsset();

	static const char* GetId() { return "asset_grenade"; }
	virtual const char* GetType() const override { return "asset_grenade"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class SmokeGrenadeAsset : public AssetMod {
public:
	SmokeGrenadeAsset();

	static const char* GetId() { return "asset_smoke_grenade"; }
	virtual const char* GetType() const override { return "asset_smoke_grenade"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
