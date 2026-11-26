#include "story/script_factory.h"
#include "common/mod_concepts.h"
#include "script_test.h"

#pragma comment(lib, "Dependence.lib")

// 把宿主的随机host和debugf输出函数绑进本DLL自己那份Dependence，见Dependence/common/mod_concepts.md
FOREVER_MOD_BIND_HOST

extern "C" __declspec(dllexport) void* GetModDependencies() {
	static std::vector<std::pair<std::string, std::vector<std::string>>> dependencies = {
		{ TestScript::GetId(), {} },
	};
	return (void*)&dependencies;
}

extern "C" __declspec(dllexport) void* GetModScripts() {
	static std::vector<std::string> mods = { TestScript::GetId() };
	return (void*)&mods;
}

extern "C" __declspec(dllexport) void RegisterModScripts(ScriptFactory* factory) {
	factory->RegisterScript(TestScript::GetId(),
		[](const std::string&) -> ScriptMod* { return new TestScript(); },
		[](ScriptMod* script) { delete script; });
}

extern "C" __declspec(dllexport) void FinishModScripts(ScriptFactory* factory) {
}
