#include "room_airport.h"

using namespace std;

int AirportRoom::count = 0;

AirportRoom::AirportRoom() : id(count++) {
}

const char* AirportRoom::GetName() {
	name = "AirportRoom" + to_string(id);
	return name.data();
}

void AirportRoom::SetProperty() {
	// 机位由AirportBuilding::Layout里的AssignParking按layout的停车位矩形生成，见building_airport.cpp。
}
