#include "terrain_mod.h"
#include "roadnet_mod.h"
#include "zone_mod.h"
#include "building_mod.h"
#include "component_mod.h"
#include "room_mod.h"
#include "job_mod.h"
#include "organization_mod.h"

#pragma comment(lib, "Dependence.lib")


using namespace std;

extern "C" __declspec(dllexport) void RegisterModTerrains(TerrainFactory* factory) {
    factory->RegisterTerrain("mod", []() {
        return make_unique<ModTerrain>();
        });
}

extern "C" __declspec(dllexport) void RegisterModRoadnets(RoadnetFactory* factory) {
    factory->RegisterRoadnet("mod", []() {
        return make_unique<ModRoadnet>();
        });
}

extern "C" __declspec(dllexport) void RegisterModZones(ZoneFactory* factory) {
    factory->RegisterZone("mod", []() {
        return make_unique<ModZone>();
        });
}

extern "C" __declspec(dllexport) void RegisterModBuildings(BuildingFactory* factory) {
    factory->RegisterBuilding("mod", []() {
        return make_unique<ModBuilding>();
        });
}

extern "C" __declspec(dllexport) void RegisterModComponents(ComponentFactory* factory) {
    factory->RegisterComponent("mod", []() {
        return make_unique<ModComponent>();
        });
}

extern "C" __declspec(dllexport) void RegisterModRooms(RoomFactory* factory) {
    factory->RegisterRoom("mod", []() {
        return make_unique<ModRoom>();
        });
}

extern "C" __declspec(dllexport) void RegisterModJobs(JobFactory* factory) {
    factory->RegisterJob("mod", []() {
        return make_unique<ModJob>();
        });
}

extern "C" __declspec(dllexport) void RegisterModOrganizations(OrganizationFactory* factory) {
    factory->RegisterOrganization("mod", []() {
        return make_unique<ModOrganization>();
        });
}
