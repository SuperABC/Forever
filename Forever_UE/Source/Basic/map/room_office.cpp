#include "room_office.h"

using namespace std;

int OfficeRoom::count = 0;

OfficeRoom::OfficeRoom() : id(count++) {
}

const char* OfficeRoom::GetName() {
	name = "OfficeRoom" + to_string(id);
	return name.data();
}

void OfficeRoom::SetProperty() {
	isWorkspace = true;
	workspaceCapacity = 5;
	rent = 2000;

	doorSpecs["room"].mesh = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/SKM_door_with_cardreader.SKM_door_with_cardreader";
	doorSpecs["room"].animClass = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/ABP_Door.ABP_Door_C";
}
