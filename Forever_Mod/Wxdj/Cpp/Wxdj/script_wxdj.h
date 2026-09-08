#pragma once

#include <string>

#include "story/script_mod.h"

// 阶段3示例mod:只实现GetId/GetType/GetName三个身份接口,用于验证Mod发现/加载/
// 注册链路。GetName()保留旧代码"剧情对话"+计数器的动态命名细节(属于Id/Type/Name
// 范畴)。真正的剧情脚本逻辑(SetScript/WrapScript/MainStory等)留到阶段4迁移story
// 系统时,对照旧工程 E:\Projects\Forever_Mods\Wxdj\Cpp\Wxdj\script_wxdj.h 补上。
class WxdjScript : public ScriptMod {
public:
	static const char* GetId() { return "wxdj"; }
	virtual const char* GetType() const override { return "wxdj"; }
	virtual const char* GetName() override {
		name = "剧情对话" + std::to_string(id);
		return name.data();
	}

private:
	static inline int count = 0;
	int id = count++;
	std::string name;
};
