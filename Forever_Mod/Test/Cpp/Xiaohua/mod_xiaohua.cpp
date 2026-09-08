#include "building_xiaohua.h"

#include "map/building_factory.h"

#pragma comment(lib, "Dependence.lib")

extern "C" __declspec(dllexport) void* GetModBuildings() {
	static std::vector<std::string> mods = { "pengzhan", "yizhong" };
	return (void*)&mods;
}

extern "C" __declspec(dllexport) void RegisterModBuildings(BuildingFactory* factory) {
	factory->RegisterBuilding(PengzhanBuilding::GetId(),
		[]() -> BuildingMod* { return new PengzhanBuilding(); },
		[](BuildingMod* building) { delete building; });
	factory->RegisterBuilding(YizhongBuilding::GetId(),
		[]() -> BuildingMod* { return new YizhongBuilding(); },
		[](BuildingMod* building) { delete building; });
}

extern "C" __declspec(dllexport) void FinishModBuildings(BuildingFactory* factory) {
	factory->CleanTemp();
}
