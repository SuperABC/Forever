#pragma once

#include <string>
#include <functional>


// NameMod：一个姓名生成算法的具体实现(比如"chinese")。这次populace域第二轮迁移时从
// 阶段3占位骨架(只有GetType()/GetName())补上真正的取名接口——照抄老工程NameMod
// (E:\Projects\Forever_UE\Source\Dependence\populace\name_mod.h)的三个纯虚方法。
//
// **这次改回老工程"传一个set结果的lambda进去"的callback写法**（之前一度改成直接按值
// 返回std::string，被指出是同一类被禁止的跨DLL模式——mod侧构造的std::string临时对象
// 按值返回穿过DLL边界，Core侧接住后可能触发跨CRT堆的析构/重新分配）：
// `std::function<void(const std::string&)>`按const&传入mod侧重写的虚方法，mod内部
// 构造好的临时std::string全程在mod自己编译的代码里构造/使用/析构，跨边界的只有回调对
// `const string&`参数的读（拷贝进调用方自己的string变量），不发生"一侧分配、另一侧
// 释放"的情况——和这个项目里`RoadnetMod::DistributeRoadnet`/`TerrainMod::
// DistributeTerrain`已经在用的std::function回调传参是同一类安全模式。
class NameMod {
public:
	NameMod() = default;
	virtual ~NameMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 从一个完整姓名里截取"姓"（用于新生儿继承父亲的姓）——老工程`ChineseName::GetSurname`
	// 假定姓氏总是姓名的第一个UTF-8字符，这次同样假定；不同取名算法（将来的英文名等）可以
	// 按自己的规则重写。截取失败（比如传入空字符串）时不调用setResult（调用方读到的结果
	// 保持初始空字符串）。
	// @setResult: 算好结果后调用一次，把结果姓字符串传进去
	virtual void GetSurname(const std::string& fullName,
		const std::function<void(const std::string&)>& setResult) const = 0;

	// 全随机生成一个姓名（先按内部规则随机选一个姓，再生成名）——allowMale/allowFemale/
	// allowNeutral是"可以从男/女/中性三个候选给定名词库里各自抽取"的独立开关（不是互斥的
	// 性别选择，一个名字可以混合多个词库），三个都是false时由具体实现自己决定兜底规则。
	// 生成失败（比如候选词库为空）时不调用setResult。
	virtual void GenerateName(bool allowMale, bool allowFemale, bool allowNeutral,
		const std::function<void(const std::string&)>& setResult) const = 0;

	// 给定一个姓，只生成名——用于新生儿继承父亲的姓时复用同一套取给定名逻辑。
	virtual void GenerateName(const std::string& surname,
		bool allowMale, bool allowFemale, bool allowNeutral,
		const std::function<void(const std::string&)>& setResult) const = 0;
};
