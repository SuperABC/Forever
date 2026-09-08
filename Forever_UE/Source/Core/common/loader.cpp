#include "loader.h"

#include <windows.h>

using namespace std;

namespace {
	const vector<ModConceptDescriptor> kModConceptDescriptors = {
		{ "Terrains",      "GetModTerrains",      "RegisterModTerrains",      "FinishModTerrains" },
		{ "Roadnets",      "GetModRoadnets",      "RegisterModRoadnets",      "FinishModRoadnets" },
		{ "Zones",         "GetModZones",         "RegisterModZones",         "FinishModZones" },
		{ "Buildings",     "GetModBuildings",     "RegisterModBuildings",     "FinishModBuildings" },
		{ "Components",    "GetModComponents",    "RegisterModComponents",    "FinishModComponents" },
		{ "Rooms",         "GetModRooms",         "RegisterModRooms",         "FinishModRooms" },
		{ "Assets",        "GetModAssets",        "RegisterModAssets",        "FinishModAssets" },
		{ "Apps",          "GetModApps",          "RegisterModApps",          "FinishModApps" },
		{ "Puzzles",       "GetModPuzzles",       "RegisterModPuzzles",       "FinishModPuzzles" },
		{ "Names",         "GetModNames",         "RegisterModNames",         "FinishModNames" },
		{ "Schedulers",    "GetModSchedulers",    "RegisterModSchedulers",    "FinishModSchedulers" },
		{ "Jobs",          "GetModJobs",          "RegisterModJobs",          "FinishModJobs" },
		{ "Calendars",     "GetModCalendars",     "RegisterModCalendars",     "FinishModCalendars" },
		{ "Organizations", "GetModOrganizations", "RegisterModOrganizations", "FinishModOrganizations" },
		{ "Scripts",       "GetModScripts",       "RegisterModScripts",       "FinishModScripts" },
		{ "Products",      "GetModProducts",      "RegisterModProducts",      "FinishModProducts" },
		{ "Storages",      "GetModStorages",      "RegisterModStorages",      "FinishModStorages" },
		{ "Manufactures",  "GetModManufactures",  "RegisterModManufactures",  "FinishModManufactures" },
		{ "Routes",        "GetModRoutes",        "RegisterModRoutes",        "FinishModRoutes" },
		{ "Stations",      "GetModStations",      "RegisterModStations",      "FinishModStations" },
		{ "Vehicles",      "GetModVehicles",      "RegisterModVehicles",      "FinishModVehicles" },
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
