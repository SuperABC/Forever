#include "terrain_mod.h"
#include "roadnet_mod.h"
#include "zone_mod.h"
#include "building_mod.h"
#include "component_mod.h"
#include "room_mod.h"
#include "job_mod.h"
#include "name_mod.h"
#include "event_mod.h"
#include "change_mod.h"
#include "organization_mod.h"

#pragma comment(lib, "Dependence.lib")


using namespace std;

extern "C" __declspec(dllexport) void RegisterModTerrains(TerrainFactory* factory) {
    factory->RegisterTerrain(ModTerrain::GetId(), []() {
        return make_unique<ModTerrain>();
        });
}

extern "C" __declspec(dllexport) void RegisterModRoadnets(RoadnetFactory* factory) {
    factory->RegisterRoadnet(ModRoadnet::GetId(), []() {
        return make_unique<ModRoadnet>();
        });
}

extern "C" __declspec(dllexport) void RegisterModZones(ZoneFactory* factory) {
    factory->RegisterZone(ModZone::GetId(),
        []() { return make_unique<ModZone>(); }, ModZone::ZoneGenerator);
}

extern "C" __declspec(dllexport) void RegisterModBuildings(BuildingFactory* factory) {
    factory->RegisterBuilding(ModZone::GetId(),
        []() { return make_unique<ModBuilding>(); }, ModBuilding::GetPower());
}

extern "C" __declspec(dllexport) void RegisterModComponents(ComponentFactory* factory) {
    factory->RegisterComponent(ModComponent::GetId(), []() {
        return make_unique<ModComponent>();
        });
}

extern "C" __declspec(dllexport) void RegisterModRooms(RoomFactory* factory) {
    factory->RegisterRoom(ModRoom::GetId(), []() {
        return make_unique<ModRoom>();
        });
}

extern "C" __declspec(dllexport) void RegisterModJobs(JobFactory* factory) {
    factory->RegisterJob(ModJob::GetId(), []() {
        return make_unique<ModJob>();
        });
}

extern "C" __declspec(dllexport) void RegisterModNames(NameFactory* factory) {
    factory->RegisterName(ModName::GetId(), []() {
        return make_unique<ModName>();
        });
}

extern "C" __declspec(dllexport) void RegisterModEvents(EventFactory* factory) {
    factory->RegisterEvent(ModEvent::GetId(), []() {
        return make_unique<ModEvent>();
        });
}

extern "C" __declspec(dllexport) void RegisterModChanges(ChangeFactory* factory) {
    factory->RegisterChange(ModChange::GetId(), []() {
        return make_unique<ModChange>();
        });
}

extern "C" __declspec(dllexport) void RegisterModOrganizations(OrganizationFactory* factory) {
    factory->RegisterOrganization(ModOrganization::GetId(), []() {
        return make_unique<ModOrganization>();
        }, ModOrganization::GetPower());
}
