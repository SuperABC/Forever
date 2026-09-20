#include "building_yuanshen.h"

#include "map/building_factory.h"

#pragma comment(lib, "Dependence.lib")

int YuanshenBuilding::count = 0;

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
