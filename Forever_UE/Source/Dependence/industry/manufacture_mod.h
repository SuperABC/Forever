#pragma once

#include "common/mod_state.h"

#include <string>
#include <unordered_map>


// 设计见manufacture_mod.md。
class ManufactureMod {
public:
	ManufactureMod() = default;
	virtual ~ManufactureMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 子类在这里填targets(不在构造函数里)。
	virtual void SetTargets() = 0;

	std::unordered_map<std::string, int> targets; // 产品type -> 每天目标产几批(正整数)

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
