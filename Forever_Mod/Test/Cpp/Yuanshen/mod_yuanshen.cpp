#include "building_yuanshen.h"

#include "map/building_factory.h"

#pragma comment(lib, "Dependence.lib")

extern "C" __declspec(dllexport) void* GetModBuildings() {
	static std::vector<std::string> mods = { "yuanshen" };
	return (void*)&mods;
}

extern "C" __declspec(dllexport) void RegisterModBuildings(BuildingFactory* factory) {
	factory->RegisterBuilding(YuanshenBuilding::GetId(),
		[]() -> BuildingMod* { return new YuanshenBuilding(); },
		[](BuildingMod* building) { delete building; });
}

extern "C" __declspec(dllexport) void FinishModBuildings(BuildingFactory* factory) {
	factory->CleanTemp();
}
