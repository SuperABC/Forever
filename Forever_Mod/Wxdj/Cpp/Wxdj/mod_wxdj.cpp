#include "script_wxdj.h"

#include "story/script_factory.h"

#pragma comment(lib, "Dependence.lib")

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
