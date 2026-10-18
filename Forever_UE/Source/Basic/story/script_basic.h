#pragma once

#include "story/script_mod.h"

#include <string>


// 纯占位的默认story实现(没有真正的剧情内容)，Basic编译为DynamicLibrary(Basic.dll)，和
// Forever_Mod下的Mod一样由Config/ModLoader在运行时扫描加载，不会静态链进Forever.Build.cs，
// 见Source/Basic/README.md。
//
// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id+GetName里现拼，和
// Source/Basic/map/terrain_basic.h/.cpp的OceanTerrain同一个模式。
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
