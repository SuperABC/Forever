#pragma once

#include "player/asset_mod.h"

#include <string>


// 阶段5-6物品/背包系统测试案例——8个具体资产：
// - ContainerAsset：背包容器(cube占位)，能背在后背。
// - WheatAsset/BeefAsset/BurgerAsset：id和Industry的WheatProduct/BeefProduct/
//   BurgerProduct共用(见Source/Basic/industry/product_basic.h)，Asset只存这个type字符串，
//   不持有Product实例，见Source/Core/player/asset.md"武器/背包系统的桥接"一节。汉堡可使用
//   (usable=true)，用完count-1到0即销毁，不做饱食度效果。
// - PistolAmmoAsset/RifleAmmoAsset：子弹，新id，没有对应Product。
// - PistolWeaponAsset/RifleWeaponAsset：id和WeaponMod的PistolWeapon/RifleWeapon共用，
//   weapon=true——只能挂左肩/右肩，不能拿在手上，见Source/Core/player/player.h的
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

private:
	static int count;
	int id;
	std::string name;
};

class WheatAsset : public AssetMod {
public:
	WheatAsset();

	static const char* GetId() { return "product_wheat"; }
	virtual const char* GetType() const override { return "product_wheat"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};

class BeefAsset : public AssetMod {
public:
	BeefAsset();

	static const char* GetId() { return "product_beef"; }
	virtual const char* GetType() const override { return "product_beef"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};

class BurgerAsset : public AssetMod {
public:
	BurgerAsset();

	static const char* GetId() { return "product_burger"; }
	virtual const char* GetType() const override { return "product_burger"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};

class PistolAmmoAsset : public AssetMod {
public:
	PistolAmmoAsset();

	static const char* GetId() { return "ammo_pistol"; }
	virtual const char* GetType() const override { return "ammo_pistol"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};

class RifleAmmoAsset : public AssetMod {
public:
	RifleAmmoAsset();

	static const char* GetId() { return "ammo_rifle"; }
	virtual const char* GetType() const override { return "ammo_rifle"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};

class PistolWeaponAsset : public AssetMod {
public:
	PistolWeaponAsset();

	static const char* GetId() { return "weapon_pistol"; }
	virtual const char* GetType() const override { return "weapon_pistol"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};

class RifleWeaponAsset : public AssetMod {
public:
	RifleWeaponAsset();

	static const char* GetId() { return "weapon_rifle"; }
	virtual const char* GetType() const override { return "weapon_rifle"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};
