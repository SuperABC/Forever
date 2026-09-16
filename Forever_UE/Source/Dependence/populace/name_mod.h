#pragma once

#include <string>

// NameMod：一个姓名生成算法的具体实现(比如"chinese")。这次populace域第二轮迁移时从
// 阶段3占位骨架(只有GetType()/GetName())补上真正的取名接口——照抄老工程NameMod
// (E:\Projects\Forever_UE\Source\Dependence\populace\name_mod.h)的三个纯虚方法，但去掉
// std::function回调+PostHandle*参数：那一套是给老工程"可能异步的UI/脚本触发"场景用的，
// 这次唯一的调用方Populace::GenerateCitizens()是纯同步调用，直接用返回值更简单，也不需要
// 引入这个项目目前完全没有的PostHandle/异步基础设施。
class NameMod {
public:
	NameMod() = default;
	virtual ~NameMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// config.json里"name_mods"数组中该mod id后面的命令行式参数字符串(如
	// "pengzhan --density 1.0"里的"--density 1.0"),由<Concept>Factory::Create<Concept>
	// 创建实例后立刻调用一次。默认空实现,不需要参数的mod不用重写,格式解析完全由重写者
	// 自己决定。
	virtual void ApplyArgs(const std::string& args) {}

	// 从一个完整姓名里截取"姓"（用于新生儿继承父亲的姓）——老工程`ChineseName::GetSurname`
	// 假定姓氏总是姓名的第一个UTF-8字符，这次同样假定；不同取名算法（将来的英文名等）可以
	// 按自己的规则重写。截取失败（比如传入空字符串）返回空字符串。
	virtual std::string GetSurname(const std::string& fullName) const = 0;

	// 全随机生成一个姓名（先按内部规则随机选一个姓，再生成名）——allowMale/allowFemale/
	// allowNeutral是"可以从男/女/中性三个候选给定名词库里各自抽取"的独立开关（不是互斥的
	// 性别选择，一个名字可以混合多个词库），三个都是false时由具体实现自己决定兜底规则。
	// 生成失败（比如候选词库为空）返回空字符串。
	virtual std::string GenerateName(bool allowMale, bool allowFemale, bool allowNeutral) const = 0;

	// 给定一个姓，只生成名——用于新生儿继承父亲的姓时复用同一套取给定名逻辑。
	virtual std::string GenerateName(const std::string& surname,
		bool allowMale, bool allowFemale, bool allowNeutral) const = 0;
};
