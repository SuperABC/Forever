#include "room_parking.h"

using namespace std;

int ParkingRoom::count = 0;

ParkingRoom::ParkingRoom() : id(count++) {
}

const char* ParkingRoom::GetName() {
	name = "ParkingRoom" + to_string(id);
	return name.data();
}

void ParkingRoom::SetProperty() {
	// 停车场只是语义类型；车位由BuildingMod::AssignParking按layout的停车位矩形生成，见building_mod.md。
}
