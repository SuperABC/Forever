#include "building_xiaohua.h"

#include "map/building_factory.h"

#pragma comment(lib, "Dependence.lib")

int PengzhanBuilding::count = 0;
int YizhongBuilding::count = 0;

extern "C" __declspec(dllexport) void* GetModBuildings() {
	static std::vector<std::string> mods = { "pengzhan", "yizhong" };
	return (void*)&mods;
}

extern "C" __declspec(dllexport) void RegisterModBuildings(BuildingFactory* factory) {
	factory->RegisterBuilding(PengzhanBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new PengzhanBuilding(); },
		[](BuildingMod* building) { delete building; },
		&PengzhanBuilding::RandomAcreage, &PengzhanBuilding::GetAcreageMin,
		&PengzhanBuilding::GetAcreageMax, &PengzhanBuilding::GetPower, &PengzhanBuilding::Assign);
	factory->RegisterBuilding(YizhongBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new YizhongBuilding(); },
		[](BuildingMod* building) { delete building; },
		&YizhongBuilding::RandomAcreage, &YizhongBuilding::GetAcreageMin,
		&YizhongBuilding::GetAcreageMax, &YizhongBuilding::GetPower, &YizhongBuilding::Assign);
}

extern "C" __declspec(dllexport) void FinishModBuildings(BuildingFactory* factory) {
}
