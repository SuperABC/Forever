#include "util.h"
#include "map.h"


using namespace std;

void Element::SetZoneId(int zoneId) {
    this->zoneId = zoneId;
}

void Element::SetBuildingId(int buildingId) {
    this->buildingId = buildingId;
}

int Element::GetZoneId() {
    return this->zoneId;
}

int Element::GetBuildingId() {
    return this->buildingId;
}

Block::Block(int x, int y) {
    elements = vector<vector<shared_ptr<Element>>>(BLOCK_SIZE,
        vector<shared_ptr<Element>>(BLOCK_SIZE, nullptr));

    for (int i = 0; i < BLOCK_SIZE; i++) {
        for (int j = 0; j < BLOCK_SIZE; j++) {
            elements[j][i] = make_shared<Element>();
        }
    }
}

Block::~Block() {

}

shared_ptr<Element> Block::GetElement(int x, int y) {
    if (CheckXY(x, y))
        return elements[y - offsetY][x - offsetX];
    else
        return nullptr;
}

bool Block::CheckXY(int x, int y) {
    if (x < offsetX)return false;
    if (y < offsetY)return false;
    if (x >= offsetX + BLOCK_SIZE)return false;
    if (y >= offsetY + BLOCK_SIZE)return false;
    return true;
}

Map::Map() {
    terrainFactory = make_shared<TerrainFactory>();
    roadnetFactory = make_shared<RoadnetFactory>();
    zoneFactory = make_shared<ZoneFactory>();
    buildingFactory = make_shared<BuildingFactory>();
    componentFactory = make_shared<ComponentFactory>();
    roomFactory = make_shared<RoomFactory>();
}

Map::~Map() {
    for (auto& mod : modHandles) {
        if (mod) {
            FreeLibrary(mod);
        }
    }
    modHandles.clear();
}

void Map::InitTerrains() {
    terrainFactory->RegisterTerrain("test", []() { return std::make_unique<TestTerrain>(); });

    HMODULE modHandle = LoadLibraryA("Mod.dll");
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModTerrainsFunc registerFunc = (RegisterModTerrainsFunc)GetProcAddress(modHandle, "RegisterModTerrains");
        if (registerFunc) {
            registerFunc(terrainFactory.get());
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
        if (terrainFactory->CheckRegistered(terrainId)) {
            auto terrain = terrainFactory->CreateTerrain(terrainId);
            debugf(("Created terrain: " + terrain->GetName() + " (ID: " + terrainId + ")\n").data());
        }
        else {
            debugf("Terrain not registered: %s\n", terrainId);
        }
    }
#endif // MOD_TEST

}

void Map::InitRoadnets() {
    roadnetFactory->RegisterRoadnet("test", []() { return std::make_unique<TestRoadnet>(); });

    HMODULE modHandle = LoadLibraryA("Mod.dll");
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModRoadnetsFunc registerFunc = (RegisterModRoadnetsFunc)GetProcAddress(modHandle, "RegisterModRoadnets");
        if (registerFunc) {
            registerFunc(roadnetFactory.get());
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
        if (roadnetFactory->CheckRegistered(roadnetId)) {
            auto roadnet = roadnetFactory->CreateRoadnet(roadnetId);
            debugf(("Created roadnet: " + roadnet->GetName() + " (ID: " + roadnetId + ")\n").data());
        }
        else {
            debugf("Roadnet not registered: %s\n", roadnetId);
        }
    }
#endif // MOD_TEST

}

void Map::InitZones() {
    zoneFactory->RegisterZone("test", []() { return std::make_unique<TestZone>(); });

    HMODULE modHandle = LoadLibraryA("Mod.dll");
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModZonesFunc registerFunc = (RegisterModZonesFunc)GetProcAddress(modHandle, "RegisterModZones");
        if (registerFunc) {
            registerFunc(zoneFactory.get());
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
        if (zoneFactory->CheckRegistered(zoneId)) {
            auto zone = zoneFactory->CreateZone(zoneId);
            debugf(("Created zone: " + zone->GetName() + " (ID: " + zoneId + ")\n").data());
        }
        else {
            debugf("Zone not registered: %s\n", zoneId);
        }
    }
#endif // MOD_TEST

}

void Map::InitBuildings() {
    buildingFactory->RegisterBuilding("test", []() { return std::make_unique<TestBuilding>(); });

    HMODULE modHandle = LoadLibraryA("Mod.dll");
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModBuildingsFunc registerFunc = (RegisterModBuildingsFunc)GetProcAddress(modHandle, "RegisterModBuildings");
        if (registerFunc) {
            registerFunc(buildingFactory.get());
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
        if (buildingFactory->CheckRegistered(buildingId)) {
            auto building = buildingFactory->CreateBuilding(buildingId);
            debugf(("Created building: " + building->GetName() + " (ID: " + buildingId + ")\n").data());
        }
        else {
            debugf("Building not registered: %s\n", buildingId);
        }
    }
#endif // MOD_TEST

}

void Map::InitComponents() {
    componentFactory->RegisterComponent("test", []() { return std::make_unique<TestComponent>(); });

    HMODULE modHandle = LoadLibraryA("Mod.dll");
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModComponentsFunc registerFunc = (RegisterModComponentsFunc)GetProcAddress(modHandle, "RegisterModComponents");
        if (registerFunc) {
            registerFunc(componentFactory.get());
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
        if (componentFactory->CheckRegistered(componentId)) {
            auto component = componentFactory->CreateComponent(componentId);
            debugf(("Created component: " + component->GetName() + " (ID: " + componentId + ")\n").data());
        }
        else {
            debugf("Component not registered: %s\n", componentId);
        }
    }
#endif // MOD_TEST

}

void Map::InitRooms() {
    roomFactory->RegisterRoom("test", []() { return std::make_unique<TestRoom>(); });

    HMODULE modHandle = LoadLibraryA("Mod.dll");
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModRoomsFunc registerFunc = (RegisterModRoomsFunc)GetProcAddress(modHandle, "RegisterModRooms");
        if (registerFunc) {
            registerFunc(roomFactory.get());
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
        if (roomFactory->CheckRegistered(roomId)) {
            auto room = roomFactory->CreateRoom(roomId);
            debugf(("Created room: " + room->GetName() + " (ID: " + roomId + ")\n").data());
        }
        else {
            debugf("Room not registered: %s\n", roomId);
        }
    }
#endif // MOD_TEST

}

int Map::Init(int blockX, int blockY) {
    // 清除已有内容
    Destroy();

    // 地图尺寸需要为正
    if (blockX < 1 || blockY < 1) {
        throw invalid_argument("Invalid map size.\n");
        return 0;
    }

    // 计算地图实际长宽
    width = blockX * BLOCK_SIZE;
    height = blockY * BLOCK_SIZE;

    // 构建区块
    blocks = vector<vector<shared_ptr<Block>>>(blockY,
        vector<shared_ptr<Block>>(blockX, nullptr));
    for (int i = 0; i < blockX; i++) {
        for (int j = 0; j < blockY; j++) {
            blocks[j][i] = make_shared<Block>(i * BLOCK_SIZE, j * BLOCK_SIZE);
        }
    }

    return 0;
}

void Map::Destroy() {

}

void Map::Tick() {

}

void Map::Load(string path) {

}

void Map::Save(string path) {

}

bool Map::CheckXY(int x, int y) {
    if (x < 0)return false;
    if (y < 0)return false;
    if (x >= width)return false;
    if (y >= height)return false;
    return true;
}



