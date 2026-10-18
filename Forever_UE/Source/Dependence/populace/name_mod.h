#pragma once

#include "common/mod_state.h"

#include <string>
#include <functional>


// NameMod：一个姓名生成算法的具体实现(比如"chinese")，三个纯虚方法构成取名接口。
//
// **必须是"传一个set结果的lambda进去"的callback写法，不能直接按值返回std::string**
// ——mod侧构造的std::string临时对象按值返回穿过DLL边界，是被禁止的跨DLL模式（Core侧
// 接住后可能触发跨CRT堆的析构/重新分配）：
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

	// 从一个完整姓名里截取"姓"（用于新生儿继承父亲的姓）——假定姓氏总是姓名的第一个
	// UTF-8字符；不同取名算法（将来的英文名等）可以
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

	// ---- 存档(见Source/Dependence/common/mod_state.md) ----
	// mod私有数据的存档/读档钩子，默认不存任何东西。基类里的公共声明字段由Core直接快照，
	// 这里只放mod自己的私有数据。
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	// SaveState写出数据的格式版本：改了自己SaveState的格式就加一，读档时从
	// StateReader::GetSavedVersion()拿到当时的版本自己迁移。默认0。
	virtual int GetStateVersion() const { return 0; }
	// 这个mod类用来生成唯一实例名的静态计数器——存档保存、读档后恢复，避免读档后新建的实例和
	// 恢复出来的名字撞车。没有计数器的类保持默认(返回-1、Set不做事)。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
