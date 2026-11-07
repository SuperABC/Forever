#pragma once

#include "populace/name_mod.h"

#include <vector>


class ChineseName : public NameMod {
public:
	ChineseName();

	static const char* GetId() { return "name_chinese"; }
	virtual const char* GetType() const override { return "name_chinese"; }
	virtual const char* GetName() override;

	virtual void GetSurname(const std::string& fullName,
		const std::function<void(const std::string&)>& setResult) const override;
	virtual void GenerateName(bool allowMale, bool allowFemale, bool allowNeutral,
		const std::function<void(const std::string&)>& setResult) const override;
	virtual void GenerateName(const std::string& surname,
		bool allowMale, bool allowFemale, bool allowNeutral,
		const std::function<void(const std::string&)>& setResult) const override;

private:
	void InitializeSurnames();
	void InitializeNames();

	std::vector<std::string> surnames;
	std::vector<std::string> maleNames;
	std::vector<std::string> femaleNames;
	std::vector<std::string> neutralNames;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name; // GetName()缓存用，是"实例编号"debug名，非生成出的人名
};
