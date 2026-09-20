#include "room_basic.h"

using namespace std;

int ResidenceRoom::count = 0;

ResidenceRoom::ResidenceRoom() : id(count++) {
	isResidential = true;
	residentialCapacity = 1;
}

const char* ResidenceRoom::GetName() {
	name = "ResidenceRoom" + to_string(id);
	return name.data();
}

int ShopRoom::count = 0;

ShopRoom::ShopRoom() : id(count++) {
	isWorkspace = true;
	workspaceCapacity = 100;
}

const char* ShopRoom::GetName() {
	name = "ShopRoom" + to_string(id);
	return name.data();
}

int WarehouseRoom::count = 0;

WarehouseRoom::WarehouseRoom() : id(count++) {
	isStorage = true;
	storageConfig = { {"shop", 100.f} };
}

const char* WarehouseRoom::GetName() {
	name = "WarehouseRoom" + to_string(id);
	return name.data();
}

int ParkingRoom::count = 0;

ParkingRoom::ParkingRoom() : id(count++) {
}

const char* ParkingRoom::GetName() {
	name = "ParkingRoom" + to_string(id);
	return name.data();
}

int FactoryRoom::count = 0;

FactoryRoom::FactoryRoom() : id(count++) {
	isManufacture = true;
	manufactureTypes = { "experience" };
}

const char* FactoryRoom::GetName() {
	name = "FactoryRoom" + to_string(id);
	return name.data();
}
