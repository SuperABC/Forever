#pragma once

#include "condition.h"

#include <string>
#include <vector>


enum CHANGE_TYPE {
	CHANGE_NOTHING_HAPPEN, // 无

	CHANGE_SET_VALUE, // 设置全局变量
	CHANGE_REMOVE_VALUE, // 移除全局变量
};

class Change {
public:
	Change(CHANGE_TYPE type);
	virtual ~Change();

	// 获取变更类型
	CHANGE_TYPE GetType();

	// 设置/获取控制条件
	void SetCondition(Condition condition);
	Condition& GetCondition();

protected:
	CHANGE_TYPE type = CHANGE_NOTHING_HAPPEN;

	Condition condition;
};

class SetValueChange : public Change {
public:
	SetValueChange(std::string name, std::string value) :
		Change(CHANGE_SET_VALUE), name(name), value(value) {
	}
	virtual ~SetValueChange() {}

	std::string GetName() { return name; }
	std::string GetValue() { return value; }

private:
	std::string name;
	std::string value;
};

class RemoveValueChange : public Change {
public:
	RemoveValueChange(std::string name) :
		Change(CHANGE_REMOVE_VALUE), name(name) {
	}
	virtual ~RemoveValueChange() {}

	std::string GetName() { return name; }

private:
	std::string name;
};