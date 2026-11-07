#pragma once

#include <string>

#include "story/script_mod.h"

// 空壳ScriptMod，设计说明见building_yuanshen.md。
class YuanshenScript : public ScriptMod {
public:
	YuanshenScript() : id(count++) { }

	static const char* GetId() { return "script_yuanshen"; }
	virtual const char* GetType() const override { return "script_yuanshen"; }
	virtual const char* GetName() override {
		name = "yuanshen" + std::to_string(id);
		return name.data();
	}

	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }

private:
	static inline int count = 0;
	int id;
	std::string name;
};
