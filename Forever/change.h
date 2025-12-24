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
	RemoveValueChange(std::string variable)
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

class SpawnNpcChange : public Change {
public:
	SpawnNpcChange() {}
	SpawnNpcChange(std::string target, std::string gender, std::string birthday)
		: target(target), gender(gender), birthday(birthday) {
	}
	virtual ~SpawnNpcChange() {}

	static std::string GetId() { return "spawn_npc"; }
	virtual std::string GetType() const override { return "spawn_npc"; }
	virtual std::string GetName() const override { return "spawn_npc"; }

	void SetTarget(std::string target) { this->target = target; }
	std::string GetTarget() { return target; }
	void SetGender(std::string gender) { this->gender = gender; }
	std::string GetGender() { return gender; }
	void SetBirthday(std::string birthday) { this->birthday = birthday; }
	std::string GetBirthday() { return birthday; }

	virtual std::vector<std::shared_ptr<Change>> ApplyChange();

private:
	std::string target;
	std::string gender;
	std::string birthday;
};

class AddOptionChange : public Change {
public:
	AddOptionChange() {}
	AddOptionChange(std::string target, std::string option)
		: target(target), option(option) {
	}
	virtual ~AddOptionChange() {}

	static std::string GetId() { return "add_option"; }
	virtual std::string GetType() const override { return "add_option"; }
	virtual std::string GetName() const override { return "add_option"; }

	void SetTarget(std::string target) { this->target = target; }
	std::string GetTarget() { return target; }
	void SetOption(std::string option) { this->option = option; }
	std::string GetOption() { return option; }

	virtual std::vector<std::shared_ptr<Change>> ApplyChange();

private:
	std::string target;
	std::string option;
};
