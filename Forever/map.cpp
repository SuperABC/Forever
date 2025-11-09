#include "util.h"
#include "error.h"
#include "map.h"
#include "json.h"

#include <algorithm>
#include <fstream>
#include <filesystem>


using namespace std;

string Element::GetTerrain() const {
    return terrain;
}

bool Element::SetTerrain(string terrain) {
    this->terrain = terrain;

    return true;
}

int Element::GetZone() const {
    return this->zone;
}

bool Element::SetZone(int zone) {
    this->zone = zone;

    return true;
}

int Element::GetBuilding() const {
    return this->building;
}

bool Element::SetBuilding(int building) {
    this->building = building;

    return true;
}

Block::Block(int x, int y) : offsetX(x), offsetY(y) {
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

string Block::GetTerrain(int x, int y) const {
    if (!CheckXY(x, y))
        return "";

    return elements[y - offsetY][x - offsetX]->GetTerrain();
}

bool Block::SetTerrain(int x, int y, std::string terrain) {
    if (!CheckXY(x, y)) {
        return false;
    }

    return elements[y - offsetY][x - offsetX]->SetTerrain(terrain);
}

bool Block::CheckXY(int x, int y) const {
    if (x < offsetX)return false;
    if (y < offsetY)return false;
    if (x >= offsetX + BLOCK_SIZE)return false;
    if (y >= offsetY + BLOCK_SIZE)return false;
    return true;
}

Map::Map() {
    terrainFactory.reset(new TerrainFactory());
    roadnetFactory.reset(new RoadnetFactory());
    zoneFactory.reset(new ZoneFactory());
    buildingFactory.reset(new BuildingFactory());
    componentFactory.reset(new ComponentFactory());
    roomFactory.reset(new RoomFactory());
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
    terrainFactory->RegisterTerrain(TestTerrain::GetId(),
        []() { return std::make_unique<TestTerrain>(); });

    HMODULE modHandle = LoadLibraryA(REPLACE_PATH("Mod.dll"));
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModTerrainsFunc registerFunc = (RegisterModTerrainsFunc)GetProcAddress(modHandle, "RegisterModTerrains");
        if (registerFunc) {
            registerFunc(terrainFactory.get());
        }
        else {
            debugf("Incorrect dll content.\n");
        }
    }
    else {
        debugf("Failed to load mod.dll.\n");
    }

#ifdef MOD_TEST
    auto terrainList = { "test", "mod" };
    for (const auto& terrainId : terrainList) {
        if (terrainFactory->CheckRegistered(terrainId)) {
            auto terrain = terrainFactory->CreateTerrain(terrainId);
            debugf(("Created terrain: " + terrain->GetName() + " (ID: " + terrainId + ").\n").data());
        }
        else {
            debugf("Terrain not registered: %s.\n", terrainId);
        }
    }
#endif // MOD_TEST

}

void Map::InitRoadnets() {
    roadnetFactory->RegisterRoadnet(TestRoadnet::GetId(), []() { return std::make_unique<TestRoadnet>(); });

    HMODULE modHandle = LoadLibraryA(REPLACE_PATH("Mod.dll"));
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModRoadnetsFunc registerFunc = (RegisterModRoadnetsFunc)GetProcAddress(modHandle, "RegisterModRoadnets");
        if (registerFunc) {
            registerFunc(roadnetFactory.get());
        }
        else {
            debugf("Incorrect dll content.\n");
        }
    }
    else {
        debugf("Failed to load mod.dll.\n");
    }

#ifdef MOD_TEST
    auto roadnetList = { "test", "mod" };
    for (const auto& roadnetId : roadnetList) {
        if (roadnetFactory->CheckRegistered(roadnetId)) {
            auto roadnet = roadnetFactory->CreateRoadnet(roadnetId);
            debugf(("Created roadnet: " + roadnet->GetName() + " (ID: " + roadnetId + ").\n").data());
        }
        else {
            debugf("Roadnet not registered: %s.\n", roadnetId);
        }
    }
#endif // MOD_TEST

}

void Map::InitZones() {
    zoneFactory->RegisterZone(TestZone::GetId(),
        []() { return std::make_unique<TestZone>(); }, TestZone::ZoneGenerator);

    HMODULE modHandle = LoadLibraryA(REPLACE_PATH("Mod.dll"));
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModZonesFunc registerFunc = (RegisterModZonesFunc)GetProcAddress(modHandle, "RegisterModZones");
        if (registerFunc) {
            registerFunc(zoneFactory.get());
        }
        else {
            debugf("Incorrect dll content.\n");
        }
    }
    else {
        debugf("Failed to load mod.dll.\n");
    }

#ifdef MOD_TEST
    auto zoneList = { "test", "mod" };
    for (const auto& zoneId : zoneList) {
        if (zoneFactory->CheckRegistered(zoneId)) {
            auto zone = zoneFactory->CreateZone(zoneId);
            debugf(("Created zone: " + zone->GetName() + " (ID: " + zoneId + ").\n").data());
        }
        else {
            debugf("Zone not registered: %s.\n", zoneId);
        }
    }
#endif // MOD_TEST

}

void Map::InitBuildings() {
    buildingFactory->RegisterBuilding(TestBuilding::GetId(), []() { return std::make_unique<TestBuilding>(); });

    HMODULE modHandle = LoadLibraryA(REPLACE_PATH("Mod.dll"));
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModBuildingsFunc registerFunc = (RegisterModBuildingsFunc)GetProcAddress(modHandle, "RegisterModBuildings");
        if (registerFunc) {
            registerFunc(buildingFactory.get());
        }
        else {
            debugf("Incorrect dll content.\n");
        }
    }
    else {
        debugf("Failed to load mod.dll.\n");
    }

#ifdef MOD_TEST
    auto buildingList = { "test", "mod" };
    for (const auto& buildingId : buildingList) {
        if (buildingFactory->CheckRegistered(buildingId)) {
            auto building = buildingFactory->CreateBuilding(buildingId);
            debugf(("Created building: " + building->GetName() + " (ID: " + buildingId + ").\n").data());
        }
        else {
            debugf("Building not registered: %s.\n", buildingId);
        }
    }
#endif // MOD_TEST

}

void Map::InitComponents() {
    componentFactory->RegisterComponent(TestComponent::GetId(), []() { return std::make_unique<TestComponent>(); });

    HMODULE modHandle = LoadLibraryA(REPLACE_PATH("Mod.dll"));
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModComponentsFunc registerFunc = (RegisterModComponentsFunc)GetProcAddress(modHandle, "RegisterModComponents");
        if (registerFunc) {
            registerFunc(componentFactory.get());
        }
        else {
            debugf("Incorrect dll content.\n");
        }
    }
    else {
        debugf("Failed to load mod.dll.\n");
    }

#ifdef MOD_TEST
    auto componentList = { "test", "mod" };
    for (const auto& componentId : componentList) {
        if (componentFactory->CheckRegistered(componentId)) {
            auto component = componentFactory->CreateComponent(componentId);
            debugf(("Created component: " + component->GetName() + " (ID: " + componentId + ").\n").data());
        }
        else {
            debugf("Component not registered: %s.\n", componentId);
        }
    }
#endif // MOD_TEST

}

void Map::InitRooms() {
    roomFactory->RegisterRoom(TestRoom::GetId(), []() { return std::make_unique<TestRoom>(); });

    HMODULE modHandle = LoadLibraryA(REPLACE_PATH("Mod.dll"));
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModRoomsFunc registerFunc = (RegisterModRoomsFunc)GetProcAddress(modHandle, "RegisterModRooms");
        if (registerFunc) {
            registerFunc(roomFactory.get());
        }
        else {
            debugf("Incorrect dll content.\n");
        }
    }
    else {
        debugf("Failed to load mod.dll.\n");
    }

#ifdef MOD_TEST
    auto roomList = { "test", "mod" };
    for (const auto& roomId : roomList) {
        if (roomFactory->CheckRegistered(roomId)) {
            auto room = roomFactory->CreateRoom(roomId);
            debugf(("Created room: " + room->GetName() + " (ID: " + roomId + ").\n").data());
        }
        else {
            debugf("Room not registered: %s.\n", roomId);
        }
    }
#endif // MOD_TEST

}

int Map::Init(int blockX, int blockY) {
    // 清除已有内容
    Destroy();

    // 地图尺寸需要为正
    if (blockX < 1 || blockY < 1) {
        THROW_EXCEPTION(InvalidArgumentException, "Invalid map size.\n");
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

    // 生成地形
    auto getTerrain = [this](int x, int y) -> string {
        return this->GetTerrain(x, y);
        };
    auto setTerrain = [this](int x, int y, const string terrain) -> bool {
        return this->SetTerrain(x, y, terrain);
        };
    auto terrains = terrainFactory->GetTerrains();
    sort(terrains.begin(), terrains.end(),
        [](const unique_ptr<Terrain>& a, const unique_ptr<Terrain>& b) {
            return a->GetPriority() < b->GetPriority();
        });
    for (auto& terrain : terrains) {
        terrain->DistributeTerrain(width, height, setTerrain, getTerrain);
    }

    // 随机生成路网
    roadnet = roadnetFactory->GetRoadnet();
    if (!roadnet) {
        THROW_EXCEPTION(InvalidConfigException, "No enabled roadnet in config.\n");
    }
    roadnet->DistributeRoadnet(width, height, getTerrain);

    // 随机生成园区
    zoneFactory->GenerateAll(roadnet->GetPlots());
    for (auto plot : roadnet->GetPlots()) {
        auto z = plot->GetZones();
        zones.insert(zones.end(), z.begin(), z.end());
    }

    // 随机生成建筑


    return 0;
}

void Map::Destroy() {
    blocks.clear();
}

void Map::Tick() {

}

void Map::Load(string path) {

}

void Map::Save(string path) {

}

void Map::ReadConfigs(std::string path) const {
    if (!filesystem::exists(path)) {
        THROW_EXCEPTION(IOException, "Path does not exist: " + path + ".\n");
    }

    Json::Reader reader;
    Json::Value root;

    ifstream fin(path);
    if (!fin.is_open()) {
        THROW_EXCEPTION(IOException, "Failed to open file: " + path + ".\n");
    }
    if (reader.parse(fin, root)) {
        for (auto terrain : root["mods"]["terrain"]) {
            terrainFactory->SetConfig(terrain.asString(), true);
        }
        roadnetFactory->SetConfig(root["mods"]["roadnet"].asString(), true);
        for (auto zone : root["mods"]["zone"]) {
            zoneFactory->SetConfig(zone.asString(), true);
        }
        for (auto building : root["mods"]["building"]) {
            buildingFactory->SetConfig(building.asString(), true);
        }
        for (auto component : root["mods"]["component"]) {
            componentFactory->SetConfig(component.asString(), true);
        }
        for (auto room : root["mods"]["room"]) {
            roomFactory->SetConfig(room.asString(), true);
        }

    }
    else {
        fin.close();
        THROW_EXCEPTION(JsonFormatException, "Json syntax error: " + reader.getFormattedErrorMessages() + ".\n");
    }
    fin.close();
}

bool Map::CheckXY(int x, int y) const {
    if (x < 0)return false;
    if (y < 0)return false;
    if (x >= width)return false;
    if (y >= height)return false;
    return true;
}

std::string Map::GetTerrain(int x, int y) const {
    if (!CheckXY(x, y)) {
        return "";
    }

    int blockX = x / BLOCK_SIZE;
    int blockY = y / BLOCK_SIZE;

    if (blockX >= blocks.size() || blockY >= blocks[0].size()) {
        return "";
    }

    return blocks[blockX][blockY]->GetTerrain(x, y);
}

bool Map::SetTerrain(int x, int y, std::string terrain) {
    if (!CheckXY(x, y)) {
        return false;
    }

    int blockX = x / BLOCK_SIZE;
    int blockY = y / BLOCK_SIZE;

    if (blockX >= blocks.size() || blockY >= blocks[0].size()) {
        return false;
    }

    return blocks[blockX][blockY]->SetTerrain(x, y, terrain);
}



