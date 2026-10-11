#pragma once

#include "populace/name_mod.h"

#include <vector>


// ChineseName：中文取名算法的具体实现——姓氏/名字词库+取名算法。接口是"传一个set
// 结果的lambda进去"的callback写法，见Source/Dependence/populace/name_mod.h的说明，
// 详见Source/Core/populace/populace.md。
// Basic编译为DynamicLibrary(Basic.dll),和Forever_Mod下的Mod一样由Config/ModLoader在
// 运行时扫描加载,不会静态链进Forever.Build.cs,见 Source/Basic/README.md。
class ChineseName : public NameMod {
public:
	ChineseName();

	static const char* GetId() { return "chinese"; }
	virtual const char* GetType() const override { return "chinese"; }
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

	static int count;
	int id;
	std::string name; // GetName()缓存用，是"实例编号"debug名，非生成出的人名
};
