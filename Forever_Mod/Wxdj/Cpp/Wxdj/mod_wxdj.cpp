#include "building_wxdj.h"

#include "map/building_factory.h"
#include "story/script_factory.h"
#include "script_wxdj.h"

#pragma comment(lib, "Dependence.lib")

int WxdjBuilding::count = 0;

// 这个DLL注册一个building mod(building_wxdj)和一个script mod(script_wxdj)。建筑用到room_empty/
// component_empty；主线脚本mod不依赖其它mod(建筑是脚本运行时按type查的，不是编译期依赖)。
extern "C" __declspec(dllexport) void* GetModDependencies() {
	static std::vector<std::pair<std::string, std::vector<std::string>>> dependencies = {
		{ WxdjBuilding::GetId(), { "room_empty", "component_empty" } },
		{ WxdjScript::GetId(), {} },
	};
	return (void*)&dependencies;
}

extern "C" __declspec(dllexport) void* GetModBuildings() {
	static std::vector<std::string> mods = { WxdjBuilding::GetId() };
	return (void*)&mods;
}

extern "C" __declspec(dllexport) void RegisterModBuildings(BuildingFactory* factory) {
	factory->RegisterBuilding(WxdjBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new WxdjBuilding(); },
		[](BuildingMod* building) { delete building; },
		&WxdjBuilding::RandomAcreage, &WxdjBuilding::GetAcreageMin,
		&WxdjBuilding::GetAcreageMax, &WxdjBuilding::GetPower, &WxdjBuilding::Assign);
}

extern "C" __declspec(dllexport) void FinishModBuildings(BuildingFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModScripts() {
	static std::vector<std::string> mods = { WxdjScript::GetId() };
	return (void*)&mods;
}

extern "C" __declspec(dllexport) void RegisterModScripts(ScriptFactory* factory) {
	factory->RegisterScript(WxdjScript::GetId(),
		[](const std::string&) -> ScriptMod* { return new WxdjScript(); },
		[](ScriptMod* script) { delete script; });
}

extern "C" __declspec(dllexport) void FinishModScripts(ScriptFactory* factory) {
}
