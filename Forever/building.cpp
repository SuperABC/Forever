#include "building.h"


using namespace std;

int DefaultResidentialBuilding::count = 0;

DefaultResidentialBuilding::DefaultResidentialBuilding() {
    name = count++;
}

string DefaultResidentialBuilding::GetId() {
    return "default_residential";
}

string DefaultResidentialBuilding::GetType() const {
    return "default_residential";
}

string DefaultResidentialBuilding::GetName() const {
    return string("默认住宅建筑") + to_string(name);
}

vector<float> DefaultResidentialBuilding::GetPower() {
    return { 0.f, 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
}

float DefaultResidentialBuilding::RandomAcreage() const {
    return 600.f * powf(1.f + GetRandom(1000) / 1000.f * 3.f, 2);
}

float DefaultResidentialBuilding::GetAcreageMin() const {
    return 600.f;
}

float DefaultResidentialBuilding::GetAcreageMax() const {
    return 9600.f;
}

Rect DefaultResidentialBuilding::LayoutConstruction() {
    if (GetAcreage() < 1000) {
        layers = 1 + GetRandom(2);
    }
    else if (GetAcreage() < 4000) {
        layers = 2 + GetRandom(3);
    }
    else {
        layers = 4 + GetRandom(4);
    }
    basements = 1;

    return Rect(0.5f * GetSizeX(), 0.5f * GetSizeY(), 0.5f * GetSizeX(), 0.5f * GetSizeY());
}

void DefaultResidentialBuilding::LayoutRooms(RoomFactory* factory, unique_ptr<Layout>& layout) {
    int direction = 0;
    if (GetSizeX() > GetSizeY()) {
        if (GetSizeY() > 3.f) {
            direction = GetRandom(2);
        }
        else {
            direction = 2 + GetRandom(2);
        }
    }
    else {
        if (GetSizeX() > 3.f) {
            direction = 2 + GetRandom(2);
        }
        else {
            direction = GetRandom(2);
        }
    }

    auto component = CreateComponent<DefaultResidentialComponent>();
    ReadFloor(-1, direction, "single_room", layout);
    AssignRoom(-1, 0, "default_residential", component, factory);
    for (int i = 0; i < layers; i++) {
        ReadFloor(i, direction, "straight_linear", layout);
        ArrangeRow(i, 0, "default_residential", 100.f, component, factory);
        ArrangeRow(i, 1, "default_residential", 100.f, component, factory);
    }
}

int DefaultWorkingBuilding::count = 0;

DefaultWorkingBuilding::DefaultWorkingBuilding() {
    name = count++;
}

string DefaultWorkingBuilding::GetId() {
    return "default_working";
}

string DefaultWorkingBuilding::GetType() const {
    return "default_working";
}

string DefaultWorkingBuilding::GetName() const {
    return string("默认工作建筑") + to_string(name);
}

vector<float> DefaultWorkingBuilding::GetPower() {
    return { 0.f, .5f, .5f, .5f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
}

float DefaultWorkingBuilding::RandomAcreage() const {
    return 1000.f * powf(1.f + GetRandom(1000) / 1000.f * 3.f, 2);
}

float DefaultWorkingBuilding::GetAcreageMin() const {
    return 1000.f;
}

float DefaultWorkingBuilding::GetAcreageMax() const {
    return 16000.f;
}

Rect DefaultWorkingBuilding::LayoutConstruction() {
    if (GetAcreage() < 3000) {
        layers = 1 + GetRandom(2);
    }
    else if (GetAcreage() < 10000) {
        layers = 2 + GetRandom(3);
    }
    else {
        layers = 4 + GetRandom(4);
    }
    basements = 1;

    return Rect(0.5f * GetSizeX(), 0.5f * GetSizeY(), 0.5f * GetSizeX(), 0.5f * GetSizeY());
}

void DefaultWorkingBuilding::LayoutRooms(RoomFactory* factory, unique_ptr<Layout>& layout) {
    int direction = 0;
    if (GetSizeX() > GetSizeY()) {
        if (GetSizeY() > 3.f) {
            direction = GetRandom(2);
        }
        else {
            direction = 2 + GetRandom(2);
        }
    }
    else {
        if (GetSizeX() > 3.f) {
            direction = 2 + GetRandom(2);
        }
        else {
            direction = GetRandom(2);
        }
    }

    auto component = CreateComponent<DefaultWorkingComponent>();
    ReadFloor(-1, direction, "single_room", layout);
    AssignRoom(-1, 0, "default_working", component, factory);
    ReadFloor(0, direction, "lobby_linear", layout);
    AssignRoom(0, 0, "default_working", component, factory);
    ArrangeRow(0, 0, "default_working", 100.f, component, factory);
    ArrangeRow(0, 1, "default_working", 100.f, component, factory);
    for (int i = 1; i < layers; i++) {
        ReadFloor(i, direction, "straight_linear", layout);
        ArrangeRow(i, 0, "default_working", 100.f, component, factory);
        ArrangeRow(i, 1, "default_working", 100.f, component, factory);
    }
}



