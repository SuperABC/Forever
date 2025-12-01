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
	SetValueChange(std::string variable, std::string value)
		: variable(variable), value(value) {
	}
	virtual ~SetValueChange() {}

	static std::string GetId() { return "set_value"; }
    virtual std::string GetType() const override { return "set_value"; }
	virtual std::string GetName() const override { return "set_value"; }

	virtual std::vector<std::shared_ptr<Change>> ApplyChange() override;

	void SetVariable(std::string variable) { this->variable = variable; }
	std::string GetVariable() { return variable; }
	void SetValue(std::string value) { this->value = value; }
	std::string GetValue() { return value; }

private:
	std::string variable;
	std::string value;
};

class RemoveValueChange : public Change {
public:
	RemoveValueChange() {}
	RemoveValueChange(std::string name)
		: variable(variable) {
	}
	virtual ~RemoveValueChange() {}

	static std::string GetId() { return "remove_value"; }
    virtual std::string GetType() const override { return "remove_value"; }
	virtual std::string GetName() const override { return "remove_value"; }

	void SetVariable(std::string variable) { this->variable = variable; }
	std::string GetVariable() { return variable; }

	virtual std::vector<std::shared_ptr<Change>> ApplyChange();

private:
	std::string variable;
};