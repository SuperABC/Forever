#pragma once

#include <string>

#include "story/script_mod.h"

// WxdjScript：五行斗诀主线的ScriptMod，只是个空壳——剧情逻辑全部写在Resource/Wxdj.script里，
// 不override WrapScript(默认原样透传)，不用C++写剧情。
class WxdjScript : public ScriptMod {
public:
	static const char* GetId() { return "script_wxdj"; }
	virtual const char* GetType() const override { return "script_wxdj"; }
	virtual const char* GetName() override {
		name = "五行斗诀" + std::to_string(id);
		return name.data();
	}

private:
	static inline int count = 0;
	int id = count++;
	std::string name;
};
