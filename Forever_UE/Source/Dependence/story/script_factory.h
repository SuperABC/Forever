#pragma once

#include "story/script_mod.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>


// 最小注册表(创建/销毁/枚举)，说明见script_factory.md。
// 所有公开方法(含析构)都是virtual、creator/deleter用裸函数指针，是跨DLL堆安全的要求，不要改，
// 原因见script_factory.md和Source/Dependence/README.md
class ScriptFactory {
public:
	using CreateFunc = ScriptMod*(*)(const std::string&);
	using DestroyFunc = void(*)(ScriptMod*);

	ScriptFactory() = default;
	virtual ~ScriptFactory() = default;

	virtual void RegisterScript(const std::string& id, CreateFunc creator, DestroyFunc deleter);

	virtual ScriptMod* CreateScript(const std::string& id);

	// 必须走注册时mod提供的deleter释放，不能Factory直接delete
	virtual void DestroyScript(ScriptMod* instance);

	virtual bool CheckRegistered(const std::string& id) const;
	virtual std::vector<std::string> GetRegisteredIds() const;

	// 当前存活的实例和它们的注册id，存档用
	virtual std::vector<std::pair<std::string, ScriptMod*>> GetLiveInstances() const;

	// 设置从config.json解析出的(id, 参数字符串)表，必须在mod的RegisterModScripts被触发之前调用
	virtual void SetModArgs(const std::unordered_map<std::string, std::string>& argsById);

private:
	// 已注册且在configuredArgs里列出才算启用
	bool IsEnabled(const std::string& id) const;

	struct Entry {
		CreateFunc creator;
		DestroyFunc deleter;
	};

	std::unordered_map<std::string, Entry> registries;
	std::unordered_map<ScriptMod*, std::string> liveInstances;
	std::unordered_map<std::string, std::string> configuredArgs;
};
