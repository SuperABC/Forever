#include "building_wxdj.h"

#include "map/building_factory.h"
#include "player/puzzle_factory.h"

#include "puzzle_wxdj.h"

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

extern "C" __declspec(dllexport) void* GetModPuzzles() {
	static std::vector<std::string> mods = { WxdjPuzzle::GetId() };
	return (void*)&mods;
}

extern "C" __declspec(dllexport) void RegisterModPuzzles(PuzzleFactory* factory) {
	factory->RegisterPuzzle(WxdjPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new WxdjPuzzle(); },
		[](PuzzleMod* puzzle) { delete puzzle; });
}

extern "C" __declspec(dllexport) void FinishModPuzzles(PuzzleFactory* factory) {
}
