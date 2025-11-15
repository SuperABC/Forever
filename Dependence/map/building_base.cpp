#include "../common/error.h"

#include "building_base.h"

#include <filesystem>
#include <string>
#include <fstream>


using namespace std;

unordered_map<string, vector<pair<Facility::FACILITY_TYPE, vector<float>>>> Building::templateFacilities = {};
unordered_map<string, vector<pair<FACE_DIRECTION, vector<float>>>> Building::templateRows = {};
unordered_map<string, vector<pair<FACE_DIRECTION, vector<float>>>> Building::templateRooms = {};
RoomFactory* Building::roomFactory = nullptr;

Facility::Facility(FACILITY_TYPE type, float x, float y, float w, float h)
    : Rect(x, y, w, h), type(type) {

}

Facility::FACILITY_TYPE Facility::getType() const {
    return type;
}

Floor::Floor(int level, float width, float height) : level(level) {
    SetVertices(0, 0, width, height);
}

Floor::~Floor() {

}

void Floor::AddFacility(Facility facility) {
    facilities.push_back(facility);
}

void Floor::AddRow(std::pair<Rect, int> row) {
    rows.push_back(row);
}

void Floor::AddRoom(std::pair<Rect, int> room) {
    rooms.push_back(room);
}

int Floor::GetLevel() const {
    return level;
}

std::vector<Facility>& Floor::GetFacilities() {
    return facilities;
}

std::vector<std::pair<Rect, int>>& Floor::GetRows() {
    return rows;
}

std::vector<std::pair<Rect, int>>& Floor::GetRooms() {
    return rooms;
}

void Building::SetParent(std::shared_ptr<Plot> plot) {
    parentPlot = plot;
}

void Building::SetParent(std::shared_ptr<Zone> zone) {
    parentZone = zone;
}

std::shared_ptr<Plot> Building::GetParentPlot() const {
    return parentPlot;
}

std::shared_ptr<Zone> Building::GetParentZone() const {
    return parentZone;
}

int Building::GetLayers() const {
    return layers;
}

int Building::GetBasements() const {
    return basements;
}

std::vector<std::shared_ptr<Component>>& Building::GetComponents() {
    return components;
}

std::vector<std::shared_ptr<Room>>& Building::GetRooms() {
    return rooms;
}

std::shared_ptr<Floor> Building::GetFloor(int level) const {
    if (basements + level >= 0 && basements + level < floors.size())
        return floors[basements + level];
    else return nullptr;
}

void Building::FinishInit() {
    floors = vector<shared_ptr<Floor>>(basements + layers);
}

void Building::ReadTemplates(string path) {
    if (!filesystem::exists(REPLACE_PATH(path))) {
        THROW_EXCEPTION(IOException, "Path does not exist: " + path + ".\n");
    }

    for (const auto& entry : filesystem::directory_iterator(path)) {
        if (entry.is_regular_file()) {
            string filename = entry.path().filename().string();
            string basename = filename.substr(0, filename.length() - 4);
            string extension = filename.substr(filename.length() - 3, filename.length());
            if (extension != "txt")continue;

            ifstream fin(entry.path());
            if (!fin.is_open()) {
                THROW_EXCEPTION(IOException, "Failed to open file: " + path + ".\n");
            }

            // 初始化当前文件的模板存储
            templateFacilities[basename] = vector<pair<Facility::FACILITY_TYPE, vector<float>>>();
            templateRows[basename] = vector<pair<FACE_DIRECTION, vector<float>>>();
            templateRooms[basename] = vector<pair<FACE_DIRECTION, vector<float>>>();

            string type;
            while (fin >> type) {
                // 处理注释行
                if (type == "#") {
                    fin.ignore((numeric_limits<streamsize>::max)(), '\n');
                    continue;
                }

                // 处理设施
                if (type == "corridor" || type == "stair" || type == "elevator") {
                    vector<float> params(8);
                    bool readError = false;

                    for (int i = 0; i < 8; i++) {
                        if (!(fin >> params[i])) {
                            readError = true;
                            break;
                        }
                    }

                    if (readError) {
                        THROW_EXCEPTION(InvalidConfigException, "Incomplete parameters for " + type +
                            " in file: " + filename + "\n");
                    }

                    // 转换为枚举类型
                    Facility::FACILITY_TYPE facType;
                    if (type == "corridor")facType = Facility::FACILITY_CORRIDOR;
                    else if (type == "stair")facType = Facility::FACILITY_STAIR;
                    else facType = Facility::FACILITY_ELEVATOR;

                    templateFacilities[basename].push_back({ facType, params });
                }
                // 处理房间排
                else if (type == "row") {
                    int directionInt;
                    if (!(fin >> directionInt)) {
                        THROW_EXCEPTION(InvalidConfigException, "Failed to read direction for row in file: " + filename + "\n");
                    }

                    // 验证方向值有效性
                    if (directionInt < 0 || directionInt > 3) {
                        THROW_EXCEPTION(InvalidConfigException, "Invalid direction value " + to_string(directionInt) +
                            " in file: " + filename + "\n");
                    }
                    FACE_DIRECTION direction = static_cast<FACE_DIRECTION>(directionInt);

                    vector<float> params(8);
                    bool readError = false;
                    for (int i = 0; i < 8; i++) {
                        if (!(fin >> params[i])) {
                            readError = true;
                            break;
                        }
                    }

                    if (readError) {
                        THROW_EXCEPTION(InvalidConfigException, "Incomplete parameters for row in file: " + filename + "\n");
                    }

                    templateRows[basename].push_back({ direction, params });
                }
                // 处理房间
                else if (type == "room") {
                    int directionInt;
                    if (!(fin >> directionInt)) {
                        THROW_EXCEPTION(InvalidConfigException, "Failed to read direction for room in file: " + filename + "\n");
                    }

                    // 验证方向值有效性
                    if (directionInt < 0 || directionInt > 3) {
                        THROW_EXCEPTION(InvalidConfigException, "Invalid direction value " + to_string(directionInt) +
                            " in file: " + filename + "\n");
                    }
                    FACE_DIRECTION direction = static_cast<FACE_DIRECTION>(directionInt);

                    vector<float> params(8);
                    bool readError = false;
                    for (int i = 0; i < 8; i++) {
                        if (!(fin >> params[i])) {
                            readError = true;
                            break;
                        }
                    }

                    if (readError) {
                        THROW_EXCEPTION(InvalidConfigException, "Incomplete parameters for room in file: " + filename + "\n");
                    }

                    templateRooms[basename].push_back({ direction, params });
                }
                // 处理未知类型
                else {
                    fin.ignore((numeric_limits<streamsize>::max)(), '\n');
                    THROW_EXCEPTION(InvalidConfigException, "Unknown type identifier '" + type +
                        "' in file: " + filename + "\n");\
                }
            }
        }
    }
}

void Building::SetFactory(RoomFactory* factory) {
    roomFactory = factory;
}

void Building::ReadFloor(int level, float width, float height, std::string name) {
    auto floor = make_shared<Floor>(level, width, height);

    for (auto facility : templateFacilities[name]) {
        float x = (facility.second[0] * width + facility.second[1] + facility.second[4] * width + facility.second[5]) / 2.f;
        float y = (facility.second[2] * height + facility.second[3] + facility.second[6] * height + facility.second[7]) / 2.f;
        float w = facility.second[4] * width + facility.second[5] - facility.second[0] * width - facility.second[1];
        float h = facility.second[6] * height + facility.second[7] - facility.second[2] * height - facility.second[3];
        floor->AddFacility(Facility(facility.first, x, y, w, h));
    }

    for (auto row : templateRows[name]) {
        float x = (row.second[0] * width + row.second[1] + row.second[4] * width + row.second[5]) / 2.f;
        float y = (row.second[2] * height + row.second[3] + row.second[6] * height + row.second[7]) / 2.f;
        float w = row.second[4] * width + row.second[5] - row.second[0] * width - row.second[1];
        float h = row.second[6] * height + row.second[7] - row.second[2] * height - row.second[3];
        floor->AddRow(make_pair(Rect(x, y, w, h), row.first));
    }

    for (auto room : templateRooms[name]) {
        float x = (room.second[0] * width + room.second[1] + room.second[4] * width + room.second[5]) / 2.f;
        float y = (room.second[2] * height + room.second[3] + room.second[6] * height + room.second[7]) / 2.f;
        float w = room.second[4] * width + room.second[5] - room.second[0] * width - room.second[1];
        float h = room.second[6] * height + room.second[7] - room.second[2] * height - room.second[3];
        floor->AddRoom(make_pair(Rect(x, y, w, h), room.first));
    }

    floors[basements + level] = floor;
}

void Building::ReadFloors(float width, float height, std::string name) {
    for (int i = 0; i < basements + layers; i++) {
        ReadFloor(i, width, height, name);
    }
}

void Building::ReadFloors(float width, float height, std::vector<std::string> names) {
    if (names.size() != basements + layers) {
        THROW_EXCEPTION(InvalidArgumentException, "Template number and building layers mismatch.\n");
    }

    for (int i = 0; i < basements + layers; i++) {
        ReadFloor(i, width, height, names[i]);
    }
}

void Building::AssignRoom(int level, int slot, std::string name, std::shared_ptr<Component> component) {
    auto room = roomFactory->CreateRoom(name);
    room->SetLayer(level);
    room->SetPosition(
        floors[basements + level]->GetRooms()[slot].first.GetPosX(),
        floors[basements + level]->GetRooms()[slot].first.GetPosY(),
        floors[basements + level]->GetRooms()[slot].first.GetSizeX(),
        floors[basements + level]->GetRooms()[slot].first.GetSizeY());
    room->SetFace(floors[basements + level]->GetRooms()[slot].second);
    rooms.push_back(std::move(room));
}

void Building::ArrangeRow(int level, int slot, std::string name, float acreage, std::shared_ptr<Component> component) {
    auto row = floors[basements + level]->GetRows()[slot];

    float num = row.first.GetAcreage() / acreage;
    if (num - (int)num >= 0.5f)num = num + 1;

    if (row.second == 0 || row.second == 1) {
        float div = row.first.GetSizeY() / (int)num;
        for (int i = 0; i <= (int)num; i++) {
            auto room = roomFactory->CreateRoom(name);
            room->SetLayer(level);
            room->SetVertices(row.first.GetLeft(), row.first.GetBottom() + div * i,
                row.first.GetRight(), row.first.GetBottom() + div * (i + 1));
            room->SetFace(floors[basements + level]->GetRows()[slot].second);
            rooms.push_back(std::move(room));
        }
    }
    else {
        float div = row.first.GetSizeX() / (int)num;
        for (int i = 0; i <= (int)num; i++) {
            auto room = roomFactory->CreateRoom(name);
            room->SetLayer(level);
            room->SetVertices(row.first.GetLeft() + div * i, row.first.GetBottom(),
                row.first.GetLeft() + div * (i + 1), row.first.GetTop());
            room->SetFace(floors[basements + level]->GetRows()[slot].second);
            rooms.push_back(std::move(room));
        }
    }
}

void BuildingFactory::RegisterBuilding(const string& id, function<unique_ptr<Building>()> creator, vector<float> power) {
    registries[id] = creator;
    configs[id] = false;
    powers[id] = power;
}

unique_ptr<Building> BuildingFactory::CreateBuilding(const string& id) {
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool BuildingFactory::CheckRegistered(const string& id) {
    return registries.find(id) != registries.end();
}

void BuildingFactory::SetConfig(std::string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}

const unordered_map<string, vector<float>>& BuildingFactory::GetPowers() const {
    return powers;
}

