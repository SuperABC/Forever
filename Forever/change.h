#pragma once

#include "change_base.h"
#include "condition.h"

#include <string>
#include <vector>


// 子类注册函数
typedef void (*RegisterModChangesFunc)(ChangeFactory* factory);

class SetValueChange : public Change {
public:
	SetValueChange() {}
	SetValueChange(std::string name, std::string value)
		: name(name), value(value) {
	}
	virtual ~SetValueChange() {}

	static std::string GetId() { return "set_value"; }
	virtual std::string GetName() const override { return "set_value"; }

	virtual std::vector<std::shared_ptr<Change>> ApplyChange();

	void SetName(std::string name) { this->name = name; }
	std::string GetName() { return name; }
	void SetValue(std::string value) { this->value = value; }
	std::string GetValue() { return value; }

private:
	std::string name;
	std::string value;
};

class RemoveValueChange : public Change {
public:
	RemoveValueChange() {}
	RemoveValueChange(std::string name)
		: name(name) {
	}
	virtual ~RemoveValueChange() {}

	static std::string GetId() { return "remove_value"; }
	virtual std::string GetName() const override { return "remove_value"; }

	void SetName(std::string name) { this->name = name; }
	std::string GetName() { return name; }

	virtual std::vector<std::shared_ptr<Change>> ApplyChange();

private:
	std::string name;
};