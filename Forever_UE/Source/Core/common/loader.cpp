#include "loader.h"

#include <windows.h>


using namespace std;

namespace {
	// 顺序即Mod screen按钮的显示顺序(人为分组，不是字母序)
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
