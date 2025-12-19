#include "../common/error.h"

#include "building_base.h"

#include <filesystem>
#include <string>
#include <fstream>


using namespace std;

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

void Floor::AddRow(pair<Rect, int> row) {
    rows.push_back(row);
}

void Floor::AddRoom(pair<Rect, int> room) {
    rooms.push_back(room);
}

int Floor::GetLevel() const {
    return level;
}

vector<Facility>& Floor::GetFacilities() {
    return facilities;
}

vector<pair<Rect, int>>& Floor::GetRows() {
    return rows;
}

vector<pair<Rect, int>>& Floor::GetRooms() {
    return rooms;
}

void Building::SetParent(shared_ptr<Plot> plot) {
    parentPlot = plot;
}

void Building::SetParent(shared_ptr<Zone> zone) {
    parentZone = zone;
}

shared_ptr<Plot> Building::GetParentPlot() const {
    return parentPlot;
}

shared_ptr<Zone> Building::GetParentZone() const {
    return parentZone;
}

int Building::GetOwner() const {
    return ownerId;
}

void Building::SetOwner(int id) {
    ownerId = id;
}

bool Building::GetStateOwned() const {
    return stateOwned;
}

void Building::SetStateOwned(bool state) {
    stateOwned = state;
}

int Building::GetLayers() const {
    return layers;
}

int Building::GetBasements() const {
    return basements;
}

const Rect Building::GetConstruction() const {
	return construction;
}

vector<shared_ptr<Component>>& Building::GetComponents() {
    return components;
}

vector<shared_ptr<Room>>& Building::GetRooms() {
    return rooms;
}

shared_ptr<Floor> Building::GetFloor(int level) const {
    if (basements + level >= 0 && basements + level < floors.size())
        return floors[basements + level];
    else return nullptr;
}

void Building::FinishInit() {
    floors = vector<shared_ptr<Floor>>(basements + layers);
	construction = LayoutConstruction();
}

unique_ptr<Layout> Building::ReadTemplates(string path) {
    if (!filesystem::exists(REPLACE_PATH(path))) {
        THROW_EXCEPTION(IOException, "Path does not exist: " + path + ".\n");
    }

    auto layout = make_unique<Layout>();

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
            layout->templateFacilities[basename] = vector<vector<pair<Facility::FACILITY_TYPE, vector<float>>>>(4);
            layout->templateRows[basename] = vector<vector<pair<FACE_DIRECTION, vector<float>>>>(4);
            layout->templateRooms[basename] = vector<vector<pair<FACE_DIRECTION, vector<float>>>>(4);

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

                    layout->templateFacilities[basename][0].push_back({ facType, InverseParams(params, 0) });
                    layout->templateFacilities[basename][1].push_back({ facType, InverseParams(params, 1) });
                    layout->templateFacilities[basename][2].push_back({ facType, InverseParams(params, 2) });
                    layout->templateFacilities[basename][3].push_back({ facType, InverseParams(params, 3) });
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

                    layout->templateRows[basename][0].push_back({ 
                        (FACE_DIRECTION)InverseDirection(direction, 0), InverseParams(params, 0) });
                    layout->templateRows[basename][1].push_back({ 
                        (FACE_DIRECTION)InverseDirection(direction, 1), InverseParams(params, 1) });
                    layout->templateRows[basename][2].push_back({ 
                        (FACE_DIRECTION)InverseDirection(direction, 2), InverseParams(params, 2) });
                    layout->templateRows[basename][3].push_back({ 
                        (FACE_DIRECTION)InverseDirection(direction, 3), InverseParams(params, 3) });
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

                    layout->templateRooms[basename][0].push_back({
                        (FACE_DIRECTION)InverseDirection(direction, 0), InverseParams(params, 0) });
                    layout->templateRooms[basename][1].push_back({
                        (FACE_DIRECTION)InverseDirection(direction, 1), InverseParams(params, 1) });
                    layout->templateRooms[basename][2].push_back({
                        (FACE_DIRECTION)InverseDirection(direction, 2), InverseParams(params, 2) });
                    layout->templateRooms[basename][3].push_back({
                        (FACE_DIRECTION)InverseDirection(direction, 3), InverseParams(params, 3) });
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

    return layout;
}

void Building::ReadFloor(int level, int face, string name, unique_ptr<Layout>& layout) {
    auto width = construction.GetSizeX();
    auto height = construction.GetSizeY();
    auto floor = make_shared<Floor>(level, width, height);

    for (auto facility : layout->templateFacilities[name][face]) {
        float x = (facility.second[0] * width + facility.second[1] + facility.second[4] * width + facility.second[5]) / 2.f;
        float y = (facility.second[2] * height + facility.second[3] + facility.second[6] * height + facility.second[7]) / 2.f;
        float w = abs(facility.second[4] * width + facility.second[5] - facility.second[0] * width - facility.second[1]);
        float h = abs(facility.second[6] * height + facility.second[7] - facility.second[2] * height - facility.second[3]);
        floor->AddFacility(Facility(facility.first, x, y, w, h));
    }

    for (auto row : layout->templateRows[name][face]) {
        float x = (row.second[0] * width + row.second[1] + row.second[4] * width + row.second[5]) / 2.f;
        float y = (row.second[2] * height + row.second[3] + row.second[6] * height + row.second[7]) / 2.f;
        float w = abs(row.second[4] * width + row.second[5] - row.second[0] * width - row.second[1]);
        float h = abs(row.second[6] * height + row.second[7] - row.second[2] * height - row.second[3]);
        floor->AddRow(make_pair(Rect(x, y, w, h), row.first));
    }

    for (auto room : layout->templateRooms[name][face]) {
        float x = (room.second[0] * width + room.second[1] + room.second[4] * width + room.second[5]) / 2.f;
        float y = (room.second[2] * height + room.second[3] + room.second[6] * height + room.second[7]) / 2.f;
        float w = abs(room.second[4] * width + room.second[5] - room.second[0] * width - room.second[1]);
        float h = abs(room.second[6] * height + room.second[7] - room.second[2] * height - room.second[3]);
        floor->AddRoom(make_pair(Rect(x, y, w, h), room.first));
    }

    floors[basements + level] = floor;
}

void Building::ReadFloors(int face, string name, unique_ptr<Layout>& layout) {
    for (int i = 0; i < basements + layers; i++) {
        ReadFloor(i, face, name, layout);
    }
}

void Building::ReadFloors(int face, vector<string> names, unique_ptr<Layout>& layout) {
    if (names.size() != basements + layers) {
        THROW_EXCEPTION(InvalidArgumentException, "Template number and building layers mismatch.\n");
    }

    for (int i = 0; i < basements + layers; i++) {
        ReadFloor(i, face, names[i], layout);
    }
}

void Building::AssignRoom(int level, int slot, string name, shared_ptr<Component> component, RoomFactory* factory) {
    shared_ptr<Room> room = factory->CreateRoom(name);
    room->SetLayer(level);
    room->SetPosition(
        floors[basements + level]->GetRooms()[slot].first.GetPosX(),
        floors[basements + level]->GetRooms()[slot].first.GetPosY(),
        floors[basements + level]->GetRooms()[slot].first.GetSizeX(),
        floors[basements + level]->GetRooms()[slot].first.GetSizeY());
    room->SetFace(floors[basements + level]->GetRooms()[slot].second);
    component->AddRoom(room);
    rooms.push_back(room);
}

void Building::ArrangeRow(int level, int slot, string name, float acreage, shared_ptr<Component> component, RoomFactory* factory) {
    auto row = floors[basements + level]->GetRows()[slot];

    float num = row.first.GetAcreage() / acreage;
    if (num - (int)num >= 0.5f)num = num + 1;

    if (row.second == 0 || row.second == 1) {
        float div = row.first.GetSizeY() / (int)num;
        for (int i = 0; i < (int)num; i++) {
            shared_ptr<Room> room = factory->CreateRoom(name);
            room->SetLayer(level);
            room->SetVertices(row.first.GetLeft(), row.first.GetBottom() + div * i,
                row.first.GetRight(), row.first.GetBottom() + div * (i + 1));
            room->SetFace(floors[basements + level]->GetRows()[slot].second);
            component->AddRoom(room);
            rooms.push_back(move(room));
        }
    }
    else {
        float div = row.first.GetSizeX() / (int)num;
        for (int i = 0; i < (int)num; i++) {
            shared_ptr<Room> room = factory->CreateRoom(name);
            room->SetLayer(level);
            room->SetVertices(row.first.GetLeft() + div * i, row.first.GetBottom(),
                row.first.GetLeft() + div * (i + 1), row.first.GetTop());
            room->SetFace(floors[basements + level]->GetRows()[slot].second);
            component->AddRoom(room);
            rooms.push_back(move(room));
        }
    }
}

vector<float> Building::InverseParams(vector<float>& params, int face) {
    if (face < 0 || face >= 4) {
        THROW_EXCEPTION(InvalidArgumentException, "Wrong facing direction.\n");
    }
    auto inversed = params;
    switch (face) {
    case 0:
        inversed[0] = params[2];
        inversed[1] = params[3];
        inversed[2] = params[0];
        inversed[3] = params[1];
        inversed[4] = params[6];
        inversed[5] = params[7];
        inversed[6] = params[4];
        inversed[7] = params[5];
        break;
    case 1:
        inversed[0] = 1.f - params[2];
        inversed[1] = -params[3];
        inversed[2] = 1.f - params[0];
        inversed[3] = -params[1];
        inversed[4] = 1.f - params[6];
        inversed[5] = -params[7];
        inversed[6] = 1.f - params[4];
        inversed[7] = -params[5];
        break;
    case 2:
        break;
    case 3:
        inversed[0] = 1.f - params[0];
        inversed[1] = -params[1];
        inversed[2] = 1.f - params[2];
        inversed[3] = -params[3];
        inversed[4] = 1.f - params[4];
        inversed[5] = -params[5];
        inversed[6] = 1.f - params[6];
        inversed[7] = -params[7];
        break;
    default:
        break;
    }

    return inversed;
}

int Building::InverseDirection(int direction, int face) {
    if (face < 0 || face >= 4) {
        THROW_EXCEPTION(InvalidArgumentException, "Wrong facing direction.\n");
    }
    switch (face) {
    case 0:
        return (direction + 2) % 4;
    case 1:
        if (direction >= 2)return 3 - direction;
        else return direction + 2;
    case 2:
        return direction;
    case 3:
        if (direction >= 2)return -direction;
        else return direction;
    }

    return direction;
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

void BuildingFactory::SetConfig(string name, bool config) {
    if (configs.find(name) != configs.end()) {
        configs[name] = config;
    }
}

const unordered_map<string, vector<float>>& BuildingFactory::GetPowers() const {
    return powers;
}

