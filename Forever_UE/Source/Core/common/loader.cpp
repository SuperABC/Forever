#include "loader.h"

#include <windows.h>


using namespace std;

namespace {
	// 顺序就是Mod screen中列21个按钮的显示顺序(ForeverModPanelWidget::NativeConstruct按
	// 这张表的顺序遍历生成)，按人为指定的分组顺序排列，不是字母序/发现顺序。
	const vector<ModConceptDescriptor> kModConceptDescriptors = {
		{ "Terrains",      "GetModTerrains",      "RegisterModTerrains",      "FinishModTerrains" },
		{ "Roadnets",      "GetModRoadnets",      "RegisterModRoadnets",      "FinishModRoadnets" },
		{ "Zones",         "GetModZones",         "RegisterModZones",         "FinishModZones" },
		{ "Buildings",     "GetModBuildings",     "RegisterModBuildings",     "FinishModBuildings" },
		{ "Components",    "GetModComponents",    "RegisterModComponents",    "FinishModComponents" },
		{ "Rooms",         "GetModRooms",         "RegisterModRooms",         "FinishModRooms" },
		{ "Names",         "GetModNames",         "RegisterModNames",         "FinishModNames" },
		{ "Schedulers",    "GetModSchedulers",    "RegisterModSchedulers",    "FinishModSchedulers" },
		{ "Jobs",          "GetModJobs",          "RegisterModJobs",          "FinishModJobs" },
		{ "Organizations", "GetModOrganizations", "RegisterModOrganizations", "FinishModOrganizations" },
		{ "Scripts",       "GetModScripts",       "RegisterModScripts",       "FinishModScripts" },
		{ "Products",      "GetModProducts",      "RegisterModProducts",      "FinishModProducts" },
		{ "Storages",      "GetModStorages",      "RegisterModStorages",      "FinishModStorages" },
		{ "Manufactures",  "GetModManufactures",  "RegisterModManufactures",  "FinishModManufactures" },
		{ "Vehicles",      "GetModVehicles",      "RegisterModVehicles",      "FinishModVehicles" },
		{ "Stations",      "GetModStations",      "RegisterModStations",      "FinishModStations" },
		{ "Routes",        "GetModRoutes",        "RegisterModRoutes",        "FinishModRoutes" },
		{ "Assets",        "GetModAssets",        "RegisterModAssets",        "FinishModAssets" },
		{ "Puzzles",       "GetModPuzzles",       "RegisterModPuzzles",       "FinishModPuzzles" },
		{ "Apps",          "GetModApps",          "RegisterModApps",          "FinishModApps" },
		// Weapons这一行之前漏掉了——Registry(registry.cpp)早就按WeaponFactory注册了这个
		// concept(Basic.dll也早就导出了GetModWeapons/RegisterModWeapons/FinishModWeapons，
		// config.json也有weapon_mods字段)，武器mod本身一直能正常加载/运行，只是这张表没有
		// 同步补上这一行，导致Config::AddDllPath的探测逻辑、以及按这张表生成UI列表的代码
		// (ForeverModPanelWidget)都看不到武器mod，见config.md。
		{ "Weapons",       "GetModWeapons",       "RegisterModWeapons",       "FinishModWeapons" },
	};
}

const vector<ModConceptDescriptor>& GetModConceptDescriptors() {
	return kModConceptDescriptors;
}

ModLoader::ModLoader() {
}

ModLoader::~ModLoader() {
	UnloadAll();
}

void* ModLoader::LoadHandle(const string& dllPath) {
	auto it = modHandles.find(dllPath);
	if (it != modHandles.end())
		return it->second;

	HMODULE handle = LoadLibraryA(dllPath.data());
	if (!handle)
		return nullptr;

	modHandles[dllPath] = handle;
	return handle;
}

void* ModLoader::GetSymbol(void* handle, const char* name) {
	return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
}

void ModLoader::UnloadAll() {
	for (auto& [path, handle] : modHandles) {
		FreeLibrary(static_cast<HMODULE>(handle));
	}
	modHandles.clear();
}
