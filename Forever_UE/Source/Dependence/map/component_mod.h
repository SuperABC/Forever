#pragma once

#include "common/mod_state.h"

#include <string>


// 设计见component_mod.md。
class ComponentMod {
public:
	ComponentMod() = default;
	virtual ~ComponentMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
