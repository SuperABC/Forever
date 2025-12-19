#include "building.h"


using namespace std;

int FlatBuilding::count = 0;

FlatBuilding::FlatBuilding() {
    name = count++;
    if (GetAcreage() < 400) {
        layers = 1 + GetRandom(4);
    }
    else if (GetAcreage() < 1000) {
        layers = 4 + GetRandom(7);
    }
    else {
        layers = 8 + GetRandom(13);
    }

    basements = 1;
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
    return 400.f * powf(1.f + GetRandom(1000) / 1000.f * 3.f, 2);
}

float FlatBuilding::GetAcreageMin() const {
    return 400.f;
}

float FlatBuilding::GetAcreageMax() const {
    return 6400.f;
}

Rect FlatBuilding::LayoutConstruction() const {
    return Rect(0.5f * GetSizeX(), 0.5f * GetSizeY(), 0.5f * GetSizeX(), 0.5f * GetSizeY());
}

void FlatBuilding::LayoutRooms(RoomFactory* factory, std::unique_ptr<Layout>& layout) {
    int direction = 0;
    if (GetSizeX() > GetSizeY()) {
        direction = GetRandom(2);
    }
    else {
        direction = 2 + GetRandom(2);
    }

    auto component = CreateComponent<TestComponent>();
    ReadFloor(-1, direction, "single_room", layout);
    AssignRoom(-1, 0, "test", component, factory);
    for (int i = 0; i < layers; i++) {
        ReadFloor(i, direction, "straight_linear", layout);
        ArrangeRow(i, 0, "test", 120.f, component, factory);
        ArrangeRow(i, 1, "test", 120.f, component, factory);
    }
}



