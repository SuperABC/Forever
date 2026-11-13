#pragma once

#include "player/asset_mod.h"

#include <string>


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
