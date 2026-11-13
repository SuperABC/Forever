#pragma once

#include "player/weapon_mod.h"


class CameraWeapon : public WeaponMod {
public:
	CameraWeapon();

	static const char* GetId() { return "weapon_camera"; }
	virtual const char* GetType() const override { return "weapon_camera"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;
	// 开火=拍照：向Core发"screenshot"请求。
	virtual void OnFire(PostHandle* post) override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
