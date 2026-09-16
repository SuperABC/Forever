#pragma once

#include "populace/name_mod.h"

#include <vector>

// ChineseName：populace域第二轮迁移，从阶段3占位骨架(NameBasic，只有GetType()/GetName())
// 换成老工程真正的中文取名算法(E:\Projects\Forever_UE\Source\Basic\populace\
// name_basic.h/.cpp)——姓氏/名字词库+取名算法逐字段/逐行照抄，接口签名按
// Source/Dependence/populace/name_mod.h这次去掉回调+PostHandle*的简化版对齐，详见
// Source/Core/populace/populace.md。
// Basic现在编译为DynamicLibrary(Basic.dll),和Forever_Mod下的Mod一样由Config/ModLoader在
// 运行时扫描加载,不会静态链进Forever.Build.cs,见 Source/Basic/README.md。
class ChineseName : public NameMod {
public:
	ChineseName();

	static const char* GetId() { return "chinese"; }
	virtual const char* GetType() const override { return "chinese"; }
	virtual const char* GetName() override;

	virtual std::string GetSurname(const std::string& fullName) const override;
	virtual std::string GenerateName(bool allowMale, bool allowFemale, bool allowNeutral) const override;
	virtual std::string GenerateName(const std::string& surname,
		bool allowMale, bool allowFemale, bool allowNeutral) const override;

private:
	void InitializeSurnames();
	void InitializeNames();

	std::vector<std::string> surnames;
	std::vector<std::string> maleNames;
	std::vector<std::string> femaleNames;
	std::vector<std::string> neutralNames;

	static int count;
	int id;
	std::string name; // GetName()缓存用，和老工程同样的"实例编号"debug名，非生成出的人名
};
