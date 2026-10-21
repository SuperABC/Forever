#pragma once

#include <string>

#include "story/script_mod.h"

// YuanshenScript：ys.script的主线ScriptMod。ys.script的逻辑(角色传送、操控空/荧)已经全部写在脚本里，
// 这里不override WrapScript(默认原样透传)，不用C++写剧情。test.script的control见Test/script_test.h。
class YuanshenScript : public ScriptMod {
public:
	YuanshenScript() { name = std::string("yuanshen") + std::to_string(count++); }

	static const char* GetId() { return "script_yuanshen"; }
	virtual const char* GetType() const override { return "script_yuanshen"; }
	virtual const char* GetName() override { return name.c_str(); }

	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }

private:
	static inline int count = 0;
	std::string name;
};
