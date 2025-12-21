#include "building.h"


using namespace std;

int FlatBuilding::count = 0;

FlatBuilding::FlatBuilding() {
    name = count++;
}

std::string FlatBuilding::GetId() {
    return "flat";
}

std::string FlatBuilding::GetType() const {
    return "flat";
}

std::string FlatBuilding::GetName() const {
    return std::string("公寓建筑") + std::to_string(name);
}

std::vector<float> FlatBuilding::GetPower() {
    return { 0.f, 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
}

float FlatBuilding::RandomAcreage() const {
    return 600.f * powf(1.f + GetRandom(1000) / 1000.f * 3.f, 2);
}

float FlatBuilding::GetAcreageMin() const {
    return 600.f;
}

float FlatBuilding::GetAcreageMax() const {
    return 9600.f;
}

Rect FlatBuilding::LayoutConstruction() {
    if (GetAcreage() < 1000) {
        layers = 1 + GetRandom(4);
    }
    else if (GetAcreage() < 4000) {
        layers = 4 + GetRandom(7);
    }
    else {
        layers = 8 + GetRandom(13);
    }
    basements = 1;

    return Rect(0.5f * GetSizeX(), 0.5f * GetSizeY(), 0.5f * GetSizeX(), 0.5f * GetSizeY());
}

void FlatBuilding::LayoutRooms(RoomFactory* factory, std::unique_ptr<Layout>& layout) {
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

    auto component = CreateComponent<FlatComponent>();
    ReadFloor(-1, direction, "single_room", layout);
    AssignRoom(-1, 0, "parking", component, factory);
    for (int i = 0; i < layers; i++) {
        ReadFloor(i, direction, "straight_linear", layout);
        ArrangeRow(i, 0, "flat", 100.f, component, factory);
        ArrangeRow(i, 1, "flat", 100.f, component, factory);
    }
}

int HotelBuilding::count = 0;

HotelBuilding::HotelBuilding() {
    name = count++;
}

std::string HotelBuilding::GetId() {
    return "hotel";
}

std::string HotelBuilding::GetType() const {
    return "hotel";
}

std::string HotelBuilding::GetName() const {
    return std::string("酒店建筑") + std::to_string(name);
}

std::vector<float> HotelBuilding::GetPower() {
    return { 0.f, .5f, .5f, .5f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
}

float HotelBuilding::RandomAcreage() const {
    return 1000.f * powf(1.f + GetRandom(1000) / 1000.f * 3.f, 2);
}

float HotelBuilding::GetAcreageMin() const {
    return 1000.f;
}

float HotelBuilding::GetAcreageMax() const {
    return 16000.f;
}

Rect HotelBuilding::LayoutConstruction() {
    if (GetAcreage() < 3000) {
        layers = 2 + GetRandom(4);
    }
    else if (GetAcreage() < 10000) {
        layers = 4 + GetRandom(7);
    }
    else {
        layers = 8 + GetRandom(13);
    }
    basements = 1;

    return Rect(0.5f * GetSizeX(), 0.5f * GetSizeY(), 0.5f * GetSizeX(), 0.5f * GetSizeY());
}

void HotelBuilding::LayoutRooms(RoomFactory* factory, std::unique_ptr<Layout>& layout) {
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

    auto component = CreateComponent<FlatComponent>();
    ReadFloor(-1, direction, "single_room", layout);
    AssignRoom(-1, 0, "parking", component, factory);
    ReadFloor(0, direction, "lobby_linear", layout);
    AssignRoom(0, 0, "lobby", component, factory);
    ArrangeRow(0, 0, "hotel", 100.f, component, factory);
    ArrangeRow(0, 1, "hotel", 100.f, component, factory);
    for (int i = 1; i < layers; i++) {
        ReadFloor(i, direction, "straight_linear", layout);
        ArrangeRow(i, 0, "hotel", 100.f, component, factory);
        ArrangeRow(i, 1, "hotel", 100.f, component, factory);
    }
}



