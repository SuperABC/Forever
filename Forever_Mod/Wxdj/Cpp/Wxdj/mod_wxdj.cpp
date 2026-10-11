#include "script_wxdj.h"

#include "story/script_factory.h"

#pragma comment(lib, "Dependence.lib")

// 这个DLL只有一个mod，不依赖任何其它mod。
extern "C" __declspec(dllexport) void* GetModDependencies() {
	static std::vector<std::pair<std::string, std::vector<std::string>>> dependencies = {
		{ WxdjScript::GetId(), {} },
	};
	return (void*)&dependencies;
}

extern "C" __declspec(dllexport) void* GetModScripts() {
	static std::vector<std::string> mods = { "wxdj" };
	return (void*)&mods;
}

extern "C" __declspec(dllexport) void RegisterModScripts(ScriptFactory* factory) {
	factory->RegisterScript(WxdjScript::GetId(),
		[](const std::string&) -> ScriptMod* { return new WxdjScript(); },
		[](ScriptMod* script) { delete script; });
}

extern "C" __declspec(dllexport) void FinishModScripts(ScriptFactory* factory) {
}
