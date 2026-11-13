#include "room_farm.h"

using namespace std;

int FarmRoom::count = 0;

FarmRoom::FarmRoom() : id(count++) {
}

const char* FarmRoom::GetName() {
	name = "FarmRoom" + to_string(id);
	return name.data();
}

void FarmRoom::SetProperty() {
	isManufacture = true;
	manufactureTypes = { "manufacture_farm" };
	isWorkspace = true;
	workspaceCapacity = 10;
	rent = 2000;
}
