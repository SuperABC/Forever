#include "building_xiaohua.h"

#include "map/building_factory.h"

#pragma comment(lib, "Dependence.lib")

int PengzhanBuilding::count = 0;
int YizhongBuilding::count = 0;

// 这个DLL注册了两个互不相关的building mod，各自声明自己的依赖——这里两个都不依赖任何
// 其它mod，但形状已经证明了per-mod粒度(不是整个DLL共用一份)。
extern "C" __declspec(dllexport) void* GetModDependencies() {
	static std::vector<std::pair<std::string, std::vector<std::string>>> dependencies = {
		{ PengzhanBuilding::GetId(), {} },
		{ YizhongBuilding::GetId(), {} },
	};
	return (void*)&dependencies;
}

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
