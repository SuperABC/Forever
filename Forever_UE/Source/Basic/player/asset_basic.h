#pragma once

#include "player/asset_mod.h"

#include <string>


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

class CameraWeaponAsset : public AssetMod {
public:
	CameraWeaponAsset();

	static const char* GetId() { return "asset_camera"; }
	virtual const char* GetType() const override { return "asset_camera"; }
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
