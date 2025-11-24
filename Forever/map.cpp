#include "utility.h"
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

string Element::GetZone() const {
    return this->zone;
}

bool Element::SetZone(string zone) {
    this->zone = zone;

    return true;
}

string Element::GetBuilding() const {
    return this->building;
}

bool Element::SetBuilding(string building) {
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

bool Block::SetTerrain(int x, int y, string terrain) {
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

shared_ptr<Element> Block::GetElement(int x, int y) {
    if (CheckXY(x, y))
        return elements[y - offsetY][x - offsetX];
    else
        return nullptr;
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
            //FreeLibrary(mod);
        }
    }
    modHandles.clear();
}

void Map::InitTerrains() {
    terrainFactory->RegisterTerrain(TestTerrain::GetId(),
        []() { return make_unique<TestTerrain>(); });

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
    roadnetFactory->RegisterRoadnet(TestRoadnet::GetId(), []() { return make_unique<TestRoadnet>(); });

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
        []() { return make_unique<TestZone>(); }, TestZone::ZoneGenerator);

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
    buildingFactory->RegisterBuilding(TestBuilding::GetId(),
        []() { return make_unique<TestBuilding>(); }, TestBuilding::GetPower());

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
    componentFactory->RegisterComponent(TestComponent::GetId(), []() { return make_unique<TestComponent>(); });

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
    roomFactory->RegisterRoom(TestRoom::GetId(), []() { return make_unique<TestRoom>(); });

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
    zoneFactory->GenerateAll(roadnet->GetPlots(), buildingFactory.get());
    for (auto plot : roadnet->GetPlots()) {
        auto zs = plot->GetZones();
        for (auto z : zs) {
            z.second->SetParent(plot);
            for (auto b : z.second->GetBuildings()) {
                b.second->SetParent(z.second);
                b.second->SetParent(plot);
            }
            if(zones.find(z.first) != zones.end()) {
                THROW_EXCEPTION(InvalidConfigException, "Duplicate zone name: " + z.first + ".\n");
            }
            zones[z.first] = z.second;
        }
    }

    // 随机生成建筑
    auto powers = buildingFactory->GetPowers();
    vector<vector<pair<string, float>>> cdfs(AREA_OFFICIAL_LOW + 1);
    for (int area = 0; area <= AREA_OFFICIAL_LOW; area++) {
        float sum = 0.f;
        for (auto power : powers) {
            sum += power.second[area];
            cdfs[area].emplace_back(power.first, sum);
        }
        if (sum == 0.f) {
            THROW_EXCEPTION(InvalidArgumentException, "No valid building for generation.\n");
        }
        for (auto& cdf : cdfs[area]) {
            cdf.second /= sum;
        }
    }
    for (auto plot : roadnet->GetPlots()) {
        float acreagePlot = plot->GetAcreage();
        for (auto zone : plot->GetZones()) {
            acreagePlot -= zone.second->GetAcreage();
        }
        if (acreagePlot <= 0.f)continue;

        float acreageTmp = 0.f;
        int attempt = 0;
        while (acreageTmp < acreagePlot) {
            if (attempt > 16)break;

            shared_ptr<Building> building;

            float rand = GetRandom(int(1e5)) / 1e5f;
            for (auto cdf : cdfs[plot->GetArea()]) {
                if (rand < cdf.second) {
                    building = buildingFactory->CreateBuilding(cdf.first);
                    break;
                }
            }

            if (!building) {
                attempt++;
                continue;
            }

            float acreageBuilding = building->RandomAcreage();
            float acreageMin = building->GetAcreageMin();
            float acreageMax = building->GetAcreageMax();
            if (acreagePlot - acreageTmp < acreageMin) {
                attempt++;
                continue;
            }
            else if (acreagePlot - acreageTmp < acreageBuilding) {
                acreageBuilding = acreagePlot - acreageTmp;
            }

            acreageTmp += acreageBuilding;
            building->SetAcreage(acreageBuilding);
            building->SetParent(plot);
            plot->AddBuilding(building->GetName(), building);
            if(buildings.find(building->GetName()) != buildings.end()) {
                THROW_EXCEPTION(InvalidConfigException, "Duplicate building name: " + building->GetName() + ".\n");
			}
			buildings[building->GetName()] = building;
        }
    }

    // 随机分布建筑与园区
    ArrangePlots();
    for (auto plot : roadnet->GetPlots()) {
        auto zones = plot->GetZones();
        for (auto zone : zones) {
            zone.second->ArrangeBuildings();
            SetZone(zone.second, zone.first);
            for (auto building : zone.second->GetBuildings()) {
                SetBuilding(building.second, building.first, make_pair(zone.second->GetLeft(), zone.second->GetBottom()));
            }
        }
        auto buildings = plot->GetBuildings();
        for (auto building : buildings) {
            SetBuilding(building.second, building.first);
        }
    }

    // 随机生成组合与房间
    layout = Building::ReadTemplates(REPLACE_PATH("../Resources/layouts/"));
    for (auto &building : buildings) {
        building.second->FinishInit();
        building.second->LayoutRooms(roomFactory.get(), layout);
        for (auto component : building.second->GetComponents()) {
            component->SetParent(building.second);
            for (auto room : component->GetRooms()) {
                room->SetParent(component);
                room->SetParent(building.second);
            }
        }
    }
    for (auto zone : zones) {
        for (auto& building : zone.second->GetBuildings()) {
            building.second->FinishInit();
            building.second->LayoutRooms(roomFactory.get(), layout);
            for (auto component : building.second->GetComponents()) {
                component->SetParent(building.second);
                for (auto room : component->GetRooms()) {
                    room->SetParent(component);
                    room->SetParent(building.second);
                }
            }
        }
    }

    return 20000;
}

void Map::ReadConfigs(string path) const {
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

void Map::Destroy() {
    blocks.clear();
}

void Map::Tick() {

}

void Map::Load(string path) {

}

void Map::Save(string path) {

}

pair<int, int> Map::GetSize() {
    return make_pair(width, height);
}

bool Map::CheckXY(int x, int y) const {
    if (x < 0)return false;
    if (y < 0)return false;
    if (x >= width)return false;
    if (y >= height)return false;
    return true;
}

shared_ptr<Block> Map::GetBlock(int x, int y) const {
    if (!CheckXY(x, y)) {
        throw invalid_argument("invalid block query position.\n");
        return nullptr;
    }

    int blockX = x / BLOCK_SIZE;
    int blockY = y / BLOCK_SIZE;

    return blocks[blockY][blockX];
}

shared_ptr<Element> Map::GetElement(int x, int y) const {
    if (!CheckXY(x, y)) {
        throw invalid_argument("invalid block query position.\n");
        return nullptr;
    }

    int blockX = x / BLOCK_SIZE;
    int blockY = y / BLOCK_SIZE;

    return blocks[blockY][blockX]->GetElement(x, y);
}

shared_ptr<Roadnet> Map::GetRoadnet() const {
    return roadnet;
}

shared_ptr<Zone> Map::GetZone(string name) {
	return zones[name];
}

shared_ptr<Building> Map::GetBuilding(string name) {
	return buildings[name];
}

void Map::SetZone(shared_ptr<Zone> zone, string name) {
    auto plot = zone->GetParent();

    auto v1 = plot->GetPosition(zone->GetPosX() + zone->GetSizeX() / 2.f, zone->GetPosY() + zone->GetSizeY() / 2.f);
    auto v2 = plot->GetPosition(zone->GetPosX() - zone->GetSizeX() / 2.f, zone->GetPosY() + zone->GetSizeY() / 2.f);
    auto v3 = plot->GetPosition(zone->GetPosX() - zone->GetSizeX() / 2.f, zone->GetPosY() - zone->GetSizeY() / 2.f);
    auto v4 = plot->GetPosition(zone->GetPosX() + zone->GetSizeX() / 2.f, zone->GetPosY() - zone->GetSizeY() / 2.f);

    vector<pair<float, float>> points = { v1, v2, v3, v4 };
    int minX = (int)points[0].first;
    int maxX = (int)points[0].first;
    int minY = (int)points[0].second;
    int maxY = (int)points[0].second;
    for (const auto& point : points) {
        minX = min(minX, (int)point.first);
        maxX = max(maxX, (int)point.first);
        minY = min(minY, (int)point.second);
        maxY = max(maxY, (int)point.second);
    }

    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            bool inside = false;
            for (size_t i = 0, j = points.size() - 1; i < points.size(); j = i++) {
                if (((points[i].second > y) != (points[j].second > y)) &&
                    (x < (points[j].first - points[i].first) * (y - points[i].second) /
                        (points[j].second - points[i].second) + points[i].first)) {
                    inside = !inside;
                }
            }

            if (inside) {
                auto element = GetElement((int)x, (int)y);
                if(element)element->SetZone(name);
            }
        }
    }
}

void Map::SetBuilding(shared_ptr<Building> building, string name) {
    auto plot = building->GetParentPlot();

    auto v1 = plot->GetPosition(building->GetPosX() + building->GetSizeX() / 2.f, building->GetPosY() + building->GetSizeY() / 2.f);
    auto v2 = plot->GetPosition(building->GetPosX() - building->GetSizeX() / 2.f, building->GetPosY() + building->GetSizeY() / 2.f);
    auto v3 = plot->GetPosition(building->GetPosX() - building->GetSizeX() / 2.f, building->GetPosY() - building->GetSizeY() / 2.f);
    auto v4 = plot->GetPosition(building->GetPosX() + building->GetSizeX() / 2.f, building->GetPosY() - building->GetSizeY() / 2.f);

    vector<pair<float, float>> points = { v1, v2, v3, v4 };
    int minX = (int)points[0].first;
    int maxX = (int)points[0].first;
    int minY = (int)points[0].second;
    int maxY = (int)points[0].second;
    for (const auto& point : points) {
        minX = min(minX, (int)point.first);
        maxX = max(maxX, (int)point.first);
        minY = min(minY, (int)point.second);
        maxY = max(maxY, (int)point.second);
    }

    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            bool inside = false;
            for (size_t i = 0, j = points.size() - 1; i < points.size(); j = i++) {
                if (((points[i].second > y) != (points[j].second > y)) &&
                    (x < (points[j].first - points[i].first) * (y - points[i].second) /
                        (points[j].second - points[i].second) + points[i].first)) {
                    inside = !inside;
                }
            }

            if (inside) {
                auto element = GetElement((int)x, (int)y);
                if (element)element->SetBuilding(name);
            }
        }
    }
}

void Map::SetBuilding(shared_ptr<Building> building, string name, pair<float, float> offset) {
    auto plot = building->GetParentPlot();

    auto v1 = plot->GetPosition(offset.first + building->GetPosX() + building->GetSizeX() / 2.f, offset.second + building->GetPosY() + building->GetSizeY() / 2.f);
    auto v2 = plot->GetPosition(offset.first + building->GetPosX() - building->GetSizeX() / 2.f, offset.second + building->GetPosY() + building->GetSizeY() / 2.f);
    auto v3 = plot->GetPosition(offset.first + building->GetPosX() - building->GetSizeX() / 2.f, offset.second + building->GetPosY() - building->GetSizeY() / 2.f);
    auto v4 = plot->GetPosition(offset.first + building->GetPosX() + building->GetSizeX() / 2.f, offset.second + building->GetPosY() - building->GetSizeY() / 2.f);

    vector<pair<float, float>> points = { v1, v2, v3, v4 };
    int minX = (int)points[0].first;
    int maxX = (int)points[0].first;
    int minY = (int)points[0].second;
    int maxY = (int)points[0].second;
    for (const auto& point : points) {
        minX = min(minX, (int)point.first);
        maxX = max(maxX, (int)point.first);
        minY = min(minY, (int)point.second);
        maxY = max(maxY, (int)point.second);
    }

    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            bool inside = false;
            for (size_t i = 0, j = points.size() - 1; i < points.size(); j = i++) {
                if (((points[i].second > y) != (points[j].second > y)) &&
                    (x < (points[j].first - points[i].first) * (y - points[i].second) /
                        (points[j].second - points[i].second) + points[i].first)) {
                    inside = !inside;
                }
            }

            if (inside) {
                auto element = GetElement((int)x, (int)y);
                if (element)element->SetBuilding(name);
            }
        }
    }
}

string Map::GetTerrain(int x, int y) const {
    if (!CheckXY(x, y)) {
        return "";
    }

    int blockX = x / BLOCK_SIZE;
    int blockY = y / BLOCK_SIZE;

    if (blockY >= blocks.size() || blockX >= blocks[0].size()) {
        return "";
    }

    return blocks[blockY][blockX]->GetTerrain(x, y);
}

bool Map::SetTerrain(int x, int y, string terrain) {
    if (!CheckXY(x, y)) {
        return false;
    }

    int blockX = x / BLOCK_SIZE;
    int blockY = y / BLOCK_SIZE;

    if (blockY >= blocks.size() || blockX >= blocks[0].size()) {
        return false;
    }

    return blocks[blockY][blockX]->SetTerrain(x, y, terrain);
}

void Map::ArrangePlots() {
    auto plots = roadnet->GetPlots();
    for (auto plot : plots) {
        auto zones = plot->GetZones();
        auto buildings = plot->GetBuildings();

        if (zones.empty() && buildings.empty()) continue;

        float acreageTotal = plot->GetAcreage();
        float acreageUsed = 0.f;

        for (const auto& zone : zones) {
            acreageUsed += zone.second->GetAcreage();
        }
        for (const auto& building : buildings) {
            acreageUsed += building.second->GetAcreage();
        }
        float acreageRemain = acreageTotal - acreageUsed;

        bool acreageAllocate = false;
        if (acreageRemain > 0) {
            for (auto& building : buildings) {
                float acreageTmp = building.second->GetAcreage();
                float acreageMax = building.second->GetAcreageMax();
                float acreageMin = building.second->GetAcreageMin();

                float acreageExpand = acreageMax - acreageTmp;

                if (acreageExpand > acreageRemain && acreageRemain > 0) {
                    float acreageNew = acreageTmp + acreageRemain;
                    if (acreageNew >= acreageMin && acreageNew <= acreageMax) {
                        building.second->SetAcreage(acreageNew);
                        acreageUsed += acreageRemain;
                        acreageRemain = 0.f;
                        acreageAllocate = true;
                        break;
                    }
                }
            }
        }

        vector<shared_ptr<Rect>> elements;
        if (acreageRemain > 0 && !acreageAllocate) {
            auto emptyRect = make_shared<Plot>();
            emptyRect->SetAcreage(acreageRemain);
            elements.push_back(emptyRect);
        }

        for (const auto& zone : zones) {
            elements.push_back(static_pointer_cast<Rect>(zone.second));
        }
        for (const auto& building : buildings) {
            elements.push_back(static_pointer_cast<Rect>(building.second));
        }

        if (elements.empty()) continue;

        sort(elements.begin(), elements.end(), [](shared_ptr<Rect> a, shared_ptr<Rect> b) {
            return a->GetAcreage() > b->GetAcreage();
            });

        Rect container = Rect(plot->GetSizeX() / 2, plot->GetSizeY() / 2, plot->GetSizeX(), plot->GetSizeY());
        if (elements.size() == 1) {
            elements[0]->SetPosition(container.GetPosX(), container.GetPosY(), container.GetSizeX(), container.GetSizeY());
        }
        else {
            class Chunk : public Rect {
            public:
                Chunk(shared_ptr<Rect> r1, shared_ptr<Rect> r2) : r1(r1), r2(r2) { acreage = r1->GetAcreage() + r2->GetAcreage(); }
                shared_ptr<Rect> r1, r2;
            };
            while (elements.size() > 2) {
                shared_ptr<Chunk> tmp = make_shared<Chunk>(elements[elements.size() - 1], elements[elements.size() - 2]);
                elements.pop_back();
                int i = (int)elements.size() - 2;
                for (; i >= 0; i--) {
                    if (tmp->GetAcreage() > elements[i]->GetAcreage()) {
                        elements[i + 1] = elements[i];
                    }
                    else {
                        elements[i + 1] = tmp;
                        break;
                    }
                }
                if (i < 0)elements[0] = tmp;
            }

            if (container.GetSizeX() > container.GetSizeY()) {
                if (GetRandom(2)) {
                    int divX = int(container.GetLeft() +
                        (container.GetRight() - container.GetLeft()) * elements[0]->GetAcreage() / container.GetAcreage());
                    if (abs(divX - container.GetLeft()) < 2)divX = (int)container.GetLeft();
                    if (abs(divX - container.GetRight()) < 2)divX = (int)container.GetRight();
                    elements[0]->SetVertices(container.GetLeft(), container.GetBottom(), (float)divX, container.GetTop());
                    elements[1]->SetVertices((float)divX, container.GetBottom(), container.GetRight(), container.GetTop());
                }
                else {
                    int divX = int(container.GetLeft() +
                        (container.GetRight() - container.GetLeft()) * elements[1]->GetAcreage() / container.GetAcreage());
                    if (abs(divX - container.GetLeft()) < 2)divX = (int)container.GetLeft();
                    if (abs(divX - container.GetRight()) < 2)divX = (int)container.GetRight();
                    elements[1]->SetVertices(container.GetLeft(), container.GetBottom(), (float)divX, container.GetTop());
                    elements[0]->SetVertices((float)divX, container.GetBottom(), container.GetRight(), container.GetTop());
                }
            }
            else {
                if (GetRandom(2)) {
                    int divY = int(container.GetBottom() +
                        (container.GetTop() - container.GetBottom()) * elements[0]->GetAcreage() / container.GetAcreage());
                    if (abs(divY - container.GetBottom()) < 2)divY = (int)container.GetBottom();
                    if (abs(divY - container.GetTop()) < 2)divY = (int)container.GetTop();
                    elements[0]->SetVertices(container.GetLeft(), container.GetBottom(), container.GetRight(), (float)divY);
                    elements[1]->SetVertices(container.GetLeft(), (float)divY, container.GetRight(), container.GetTop());
                }
                else {
                    int divY = int(container.GetBottom() +
                        (container.GetTop() - container.GetBottom()) * elements[1]->GetAcreage() / container.GetAcreage());
                    if (abs(divY - container.GetBottom()) < 2)divY = (int)container.GetBottom();
                    if (abs(divY - container.GetTop()) < 2)divY = (int)container.GetTop();
                    elements[1]->SetVertices(container.GetLeft(), container.GetBottom(), container.GetRight(), (float)divY);
                    elements[0]->SetVertices(container.GetLeft(), (float)divY, container.GetRight(), container.GetTop());
                }
            }

            while (elements.size() > 0) {
                auto tmp = elements.back();
                elements.pop_back();
                if (auto chunk = dynamic_pointer_cast<Chunk>(tmp)) {
                    shared_ptr<Rect> rect1 = chunk->r1;
                    shared_ptr<Rect> rect2 = chunk->r2;

                    if (tmp->GetAcreage() > 0) {
                        if (tmp->GetSizeX() > tmp->GetSizeY()) {
                            if (GetRandom(2)) {
                                int divX = int(tmp->GetLeft() +
                                    tmp->GetSizeX() * rect1->GetAcreage() / tmp->GetAcreage());
                                if (abs(divX - tmp->GetLeft()) < 2)divX = (int)tmp->GetLeft();
                                if (abs(divX - tmp->GetRight()) < 2)divX = (int)tmp->GetRight();
                                rect1->SetVertices(tmp->GetLeft(), tmp->GetBottom(), (float)divX, tmp->GetTop());
                                rect2->SetVertices((float)divX, tmp->GetBottom(), tmp->GetRight(), tmp->GetTop());
                            }
                            else {
                                int divX = int(tmp->GetLeft() +
                                    tmp->GetSizeX() * rect2->GetAcreage() / tmp->GetAcreage());
                                if (abs(divX - tmp->GetLeft()) < 2)divX = (int)tmp->GetLeft();
                                if (abs(divX - tmp->GetRight()) < 2)divX = (int)tmp->GetRight();
                                rect2->SetVertices(tmp->GetLeft(), tmp->GetBottom(), (float)divX, tmp->GetTop());
                                rect1->SetVertices((float)divX, tmp->GetBottom(), tmp->GetRight(), tmp->GetTop());
                            }
                        }
                        else {
                            if (GetRandom(2)) {
                                int divY = int(tmp->GetBottom() +
                                    tmp->GetSizeY() * rect1->GetAcreage() / tmp->GetAcreage());
                                if (abs(divY - tmp->GetBottom()) < 2)divY = (int)tmp->GetBottom();
                                if (abs(divY - tmp->GetTop()) < 2)divY = (int)tmp->GetTop();
                                rect1->SetVertices(tmp->GetLeft(), tmp->GetBottom(), tmp->GetRight(), (float)divY);
                                rect2->SetVertices(tmp->GetLeft(), (float)divY, tmp->GetRight(), tmp->GetTop());
                            }
                            else {
                                int divY = int(tmp->GetBottom() +
                                    tmp->GetSizeY() * rect2->GetAcreage() / tmp->GetAcreage());
                                if (abs(divY - tmp->GetBottom()) < 2)divY = (int)tmp->GetBottom();
                                if (abs(divY - tmp->GetTop()) < 2)divY = (int)tmp->GetTop();
                                rect2->SetVertices(tmp->GetLeft(), tmp->GetBottom(), tmp->GetRight(), (float)divY);
                                rect1->SetVertices(tmp->GetLeft(), (float)divY, tmp->GetRight(), tmp->GetTop());
                            }
                        }
                        if (dynamic_pointer_cast<Chunk>(rect1))elements.push_back(rect1);
                        if (dynamic_pointer_cast<Chunk>(rect2))elements.push_back(rect2);
                    }
                }
            }
        }
    }
}



