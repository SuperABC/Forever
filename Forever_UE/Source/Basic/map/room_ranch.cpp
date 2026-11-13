#include "room_ranch.h"

using namespace std;

int RanchRoom::count = 0;

RanchRoom::RanchRoom() : id(count++) {
}

const char* RanchRoom::GetName() {
	name = "RanchRoom" + to_string(id);
	return name.data();
}

void RanchRoom::SetProperty() {
	isManufacture = true;
	manufactureTypes = { "manufacture_ranch" };
	isWorkspace = true;
	workspaceCapacity = 10;
	rent = 2000;
}
