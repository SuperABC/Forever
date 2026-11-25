#include "mod_concepts.h"

#include <cctype>


using namespace std;

namespace {
	const vector<ModConceptDescriptor> kModConceptDescriptors = {
		{ "Terrains",      "GetModTerrains",      "RegisterModTerrains",      "FinishModTerrains" },
		{ "Roadnets",      "GetModRoadnets",      "RegisterModRoadnets",      "FinishModRoadnets" },
		{ "Zones",         "GetModZones",         "RegisterModZones",         "FinishModZones" },
		{ "Greens",        "GetModGreens",        "RegisterModGreens",        "FinishModGreens" },
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
		{ "Parties",       "GetModParties",       "RegisterModParties",       "FinishModParties" },
	};
}

const vector<ModConceptDescriptor>& GetModConceptDescriptors() {
	return kModConceptDescriptors;
}

string ModConceptJsonKey(const string& conceptKey) {
	string singular;
	if (conceptKey.size() > 3 && conceptKey.compare(conceptKey.size() - 3, 3, "ies") == 0) {
		singular = conceptKey.substr(0, conceptKey.size() - 3) + "y";
	}
	else {
		singular = conceptKey.empty() ? conceptKey : conceptKey.substr(0, conceptKey.size() - 1);
	}
	for (char& c : singular) {
		c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
	}
	return singular + "_mods";
}
