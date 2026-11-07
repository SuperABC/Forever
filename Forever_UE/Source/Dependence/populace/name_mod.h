#pragma once

#include "common/mod_state.h"

#include <string>
#include <functional>


// 设计见name_mod.md。结果必须通过setResult回调传出,不能按值返回std::string(跨DLL安全)。
class NameMod {
public:
	NameMod() = default;
	virtual ~NameMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 从完整姓名截取"姓"(新生儿继承父亲的姓用);失败(如空字符串)时不调用setResult。
	virtual void GetSurname(const std::string& fullName,
		const std::function<void(const std::string&)>& setResult) const = 0;

	// 全随机生成姓名;allowMale/allowFemale/allowNeutral是独立开关(可混合多个词库),
	// 都为false时由实现兜底;失败(词库为空)时不调用setResult。
	virtual void GenerateName(bool allowMale, bool allowFemale, bool allowNeutral,
		const std::function<void(const std::string&)>& setResult) const = 0;

	// 给定姓只生成名。
	virtual void GenerateName(const std::string& surname,
		bool allowMale, bool allowFemale, bool allowNeutral,
		const std::function<void(const std::string&)>& setResult) const = 0;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
