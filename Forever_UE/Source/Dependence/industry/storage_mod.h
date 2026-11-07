#pragma once

#include "common/mod_state.h"

#include <string>
#include <utility>
#include <vector>


// 设计见storage_mod.md。
class StorageMod {
public:
	StorageMod() = default;
	virtual ~StorageMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 子类在这里填下面的字段(不在构造函数里),Core创建实例后立刻调一次。
	virtual void SetProperty() = 0;

	std::vector<std::string> categories;
	float capacity = 0.f;

	// 物流权限,语义见storage_mod.md。
	bool canTake = true;
	bool canPut = true;

	// 补货需求,比例取[0,1](占capacity),二选一(同时非空只认productDemand),语义见storage_mod.md。
	std::vector<std::pair<std::string, float>> serviceTypeDemand;
	std::vector<std::pair<std::string, float>> productDemand;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
