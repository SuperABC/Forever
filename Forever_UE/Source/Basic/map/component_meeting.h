#pragma once

#include "map/component_mod.h"

#include <string>


// 党派会议室所属的component，无字段无逻辑的占位，详见component_meeting.md。
class MeetingComponent : public ComponentMod {
public:
	FOREVER_MOD_GENERIC_UNIT_TEST("component")
	MeetingComponent();

	static const char* GetId() { return "component_meeting"; }
	virtual const char* GetType() const override { return "component_meeting"; }
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
