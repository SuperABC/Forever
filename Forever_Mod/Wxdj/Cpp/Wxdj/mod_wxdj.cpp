#include "building_wxdj.h"

#include "map/building_factory.h"

#pragma comment(lib, "Dependence.lib")

int WxdjBuilding::count = 0;

// 这个DLL只注册一个building mod(building_wxdj)，用到room_empty/component_empty。剧情没有自己的
// ScriptMod，config.json的main_story用script_empty，逻辑全在Resource/Wxdj.script。
extern "C" __declspec(dllexport) void* GetModDependencies() {
	static std::vector<std::pair<std::string, std::vector<std::string>>> dependencies = {
		{ WxdjBuilding::GetId(), { "room_empty", "component_empty" } },
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
