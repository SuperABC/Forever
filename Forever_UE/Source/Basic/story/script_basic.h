#pragma once

#include "story/script_mod.h"

#include <string>


class ScriptBasic : public ScriptMod {
public:
	ScriptBasic();

	static const char* GetId() { return "script_basic"; }
	virtual const char* GetType() const override { return "script_basic"; }
	virtual const char* GetName() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
