#include "util.h"
#include "map.h"


using namespace std;

Map::Map() {

}

Map::~Map() {
    for (auto& mod : modHandles) {
        if (mod) {
            FreeLibrary(mod);
        }
    }
    modHandles.clear();
}

void Map::InitTerrains(std::shared_ptr<TerrainFactory> factory) {
    factory->RegisterTerrain("test", []() { return std::make_unique<TestTerrain>(); });

    HMODULE modHandle = LoadLibraryA("Mod.dll");
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModTerrainsFunc registerFunc = (RegisterModTerrainsFunc)GetProcAddress(modHandle, "RegisterModTerrains");
        if (registerFunc) {
            registerFunc(factory.get());
        }
        else {
            debugf("Incorrect dll content.");
        }
    }
    else {
        debugf("Failed to load mod.dll.");
    }

#ifdef MOD_TEST
    auto terrainList = { "test", "mod" };
    for (const auto& terrainId : terrainList) {
        if (factory->CheckRegistered(terrainId)) {
            auto terrain = factory->CreateTerrain(terrainId);
            debugf(("Created terrain: " + terrain->GetName() + " (ID: " + terrainId + ")\n").data());
        }
        else {
            debugf("Terrain not registered: %s\n", terrainId);
        }
    }
#endif // MOD_TEST

}

void Map::InitRoadnets(std::shared_ptr<RoadnetFactory> factory) {
    factory->RegisterRoadnet("test", []() { return std::make_unique<TestRoadnet>(); });

    HMODULE modHandle = LoadLibraryA("Mod.dll");
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModRoadnetsFunc registerFunc = (RegisterModRoadnetsFunc)GetProcAddress(modHandle, "RegisterModRoadnets");
        if (registerFunc) {
            registerFunc(factory.get());
        }
        else {
            debugf("Incorrect dll content.");
        }
    }
    else {
        debugf("Failed to load mod.dll.");
    }

#ifdef MOD_TEST
    auto roadnetList = { "test", "mod" };
    for (const auto& roadnetId : roadnetList) {
        if (factory->CheckRegistered(roadnetId)) {
            auto roadnet = factory->CreateRoadnet(roadnetId);
            debugf(("Created roadnet: " + roadnet->GetName() + " (ID: " + roadnetId + ")\n").data());
        }
        else {
            debugf("Roadnet not registered: %s\n", roadnetId);
        }
    }
#endif // MOD_TEST

}

void Map::InitZones(std::shared_ptr<ZoneFactory> factory) {
    factory->RegisterZone("test", []() { return std::make_unique<TestZone>(); });

    HMODULE modHandle = LoadLibraryA("Mod.dll");
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModZonesFunc registerFunc = (RegisterModZonesFunc)GetProcAddress(modHandle, "RegisterModZones");
        if (registerFunc) {
            registerFunc(factory.get());
        }
        else {
            debugf("Incorrect dll content.");
        }
    }
    else {
        debugf("Failed to load mod.dll.");
    }

#ifdef MOD_TEST
    auto zoneList = { "test", "mod" };
    for (const auto& zoneId : zoneList) {
        if (factory->CheckRegistered(zoneId)) {
            auto zone = factory->CreateZone(zoneId);
            debugf(("Created zone: " + zone->GetName() + " (ID: " + zoneId + ")\n").data());
        }
        else {
            debugf("Zone not registered: %s\n", zoneId);
        }
    }
#endif // MOD_TEST

}

void Map::InitBuildings(std::shared_ptr<BuildingFactory> factory) {
    factory->RegisterBuilding("test", []() { return std::make_unique<TestBuilding>(); });

    HMODULE modHandle = LoadLibraryA("Mod.dll");
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModBuildingsFunc registerFunc = (RegisterModBuildingsFunc)GetProcAddress(modHandle, "RegisterModBuildings");
        if (registerFunc) {
            registerFunc(factory.get());
        }
        else {
            debugf("Incorrect dll content.");
        }
    }
    else {
        debugf("Failed to load mod.dll.");
    }

#ifdef MOD_TEST
    auto buildingList = { "test", "mod" };
    for (const auto& buildingId : buildingList) {
        if (factory->CheckRegistered(buildingId)) {
            auto building = factory->CreateBuilding(buildingId);
            debugf(("Created building: " + building->GetName() + " (ID: " + buildingId + ")\n").data());
        }
        else {
            debugf("Building not registered: %s\n", buildingId);
        }
    }
#endif // MOD_TEST

}

void Map::InitComponents(std::shared_ptr<ComponentFactory> factory) {
    factory->RegisterComponent("test", []() { return std::make_unique<TestComponent>(); });

    HMODULE modHandle = LoadLibraryA("Mod.dll");
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModComponentsFunc registerFunc = (RegisterModComponentsFunc)GetProcAddress(modHandle, "RegisterModComponents");
        if (registerFunc) {
            registerFunc(factory.get());
        }
        else {
            debugf("Incorrect dll content.");
        }
    }
    else {
        debugf("Failed to load mod.dll.");
    }

#ifdef MOD_TEST
    auto componentList = { "test", "mod" };
    for (const auto& componentId : componentList) {
        if (factory->CheckRegistered(componentId)) {
            auto component = factory->CreateComponent(componentId);
            debugf(("Created component: " + component->GetName() + " (ID: " + componentId + ")\n").data());
        }
        else {
            debugf("Component not registered: %s\n", componentId);
        }
    }
#endif // MOD_TEST

}

void Map::InitRooms(std::shared_ptr<RoomFactory> factory) {
    factory->RegisterRoom("test", []() { return std::make_unique<TestRoom>(); });

    HMODULE modHandle = LoadLibraryA("Mod.dll");
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModRoomsFunc registerFunc = (RegisterModRoomsFunc)GetProcAddress(modHandle, "RegisterModRooms");
        if (registerFunc) {
            registerFunc(factory.get());
        }
        else {
            debugf("Incorrect dll content.");
        }
    }
    else {
        debugf("Failed to load mod.dll.");
    }

#ifdef MOD_TEST
    auto roomList = { "test", "mod" };
    for (const auto& roomId : roomList) {
        if (factory->CheckRegistered(roomId)) {
            auto room = factory->CreateRoom(roomId);
            debugf(("Created room: " + room->GetName() + " (ID: " + roomId + ")\n").data());
        }
        else {
            debugf("Room not registered: %s\n", roomId);
        }
    }
#endif // MOD_TEST

}
