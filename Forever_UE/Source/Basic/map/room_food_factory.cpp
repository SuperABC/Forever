#include "room_food_factory.h"

using namespace std;

int FoodFactoryRoom::count = 0;

FoodFactoryRoom::FoodFactoryRoom() : id(count++) {
}

const char* FoodFactoryRoom::GetName() {
	name = "FoodFactoryRoom" + to_string(id);
	return name.data();
}

void FoodFactoryRoom::SetProperty() {
	isManufacture = true;
	manufactureTypes = { "manufacture_food" };
	isWorkspace = true;
	workspaceCapacity = 10;
	rent = 2000;
}
