#pragma once

#include <stdexcept>
#include <string>

#include "common/error.h"
#include "player/puzzle_mod.h"

// 故障注入用的小游戏，设计说明见puzzle_fault.md。
// 它不是给玩家玩的：CoreTest的fault套件用它验证"mod里抛出的异常能被宿主正确接住并指名到这个mod"。
// SetParam指定在Init里怎么出错；不给参数(或给别的)就什么都不做，所以ModTest的通用检查照常通过。
class FaultPuzzle : public PuzzleMod {
public:
	FOREVER_MOD_GENERIC_UNIT_TEST("puzzle")
	FaultPuzzle() : id(count++) { }

	static const char* GetId() { return "puzzle_fault"; }
	virtual const char* GetType() const override { return "puzzle_fault"; }
	virtual const char* GetName() override {
		name = "fault" + std::to_string(id); // 每个实例的名字要不同(ModTest的通用检查)
		return name.data();
	}

	virtual void SetParam(const std::string& param) override { mode = param; }

	virtual void Init(Canvas* canvas, PostHandle* post) override {
		// 消息都故意写得比15个字符长：短字符串放在对象内部，长的才会在堆上分配，
		// "一侧分配、另一侧释放"的问题只有长字符串才会暴露(CONVENTIONS.md第4节)。
		if (mode == "std") throw std::runtime_error("puzzle_fault threw a std::runtime_error on purpose");
		if (mode == "data") THROW_EXCEPTION(DataException, "puzzle_fault threw a DataException on purpose");
		if (mode == "unknown") throw 42;
	}

	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override { return nullptr; }

	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }

private:
	static inline int count = 0;
	int id;
	std::string name;
	std::string mode;
};
