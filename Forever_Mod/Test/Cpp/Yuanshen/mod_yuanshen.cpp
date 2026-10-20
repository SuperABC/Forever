#include "building_yuanshen.h"

#include "map/building_factory.h"
#include "story/script_factory.h"
#include "script_yuanshen.h"

#pragma comment(lib, "Dependence.lib")

int YuanshenBuilding::count = 0;

// 这个DLL注册一个building mod和一个script mod，id都是"yuanshen"(分属不同factory)。建筑用到
// room/component "empty"，所以依赖empty；YuanshenScript要找yuanshen建筑，所以依赖building yuanshen。
extern "C" __declspec(dllexport) void* GetModDependencies() {
	static std::vector<std::pair<std::string, std::vector<std::string>>> dependencies = {
		{ YuanshenBuilding::GetId(), { "empty" } },
		{ YuanshenScript::GetId(), { YuanshenBuilding::GetId() } },
	};
	return (void*)&dependencies;
}

extern "C" __declspec(dllexport) void* GetModBuildings() {
	static std::vector<std::string> mods = { "yuanshen" };
	return (void*)&mods;
}

extern "C" __declspec(dllexport) void RegisterModBuildings(BuildingFactory* factory) {
	factory->RegisterBuilding(YuanshenBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new YuanshenBuilding(); },
		[](BuildingMod* building) { delete building; },
		&YuanshenBuilding::RandomAcreage, &YuanshenBuilding::GetAcreageMin,
		&YuanshenBuilding::GetAcreageMax, &YuanshenBuilding::GetPower, &YuanshenBuilding::Assign);
}

extern "C" __declspec(dllexport) void FinishModBuildings(BuildingFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModScripts() {
	static std::vector<std::string> mods = { "yuanshen" };
	return (void*)&mods;
}

extern "C" __declspec(dllexport) void RegisterModScripts(ScriptFactory* factory) {
	factory->RegisterScript(YuanshenScript::GetId(),
		[](const std::string&) -> ScriptMod* { return new YuanshenScript(); },
		[](ScriptMod* script) { delete script; });
}

extern "C" __declspec(dllexport) void FinishModScripts(ScriptFactory* factory) {
}
