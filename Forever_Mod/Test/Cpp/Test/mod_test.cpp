#include "story/script_factory.h"
#include "script_test.h"

#pragma comment(lib, "Dependence.lib")

// 这个DLL只注册一个script mod(script_test)，是test.script的主线mod，不依赖其他mod。
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
