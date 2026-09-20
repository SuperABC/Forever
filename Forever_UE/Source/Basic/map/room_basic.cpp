#include "room_basic.h"


ResidenceRoom::ResidenceRoom() {
	isResidential = true;
	residentialCapacity = 1;
}

ShopRoom::ShopRoom() {
	isWorkspace = true;
	workspaceCapacity = 100;
}

WarehouseRoom::WarehouseRoom() {
	isStorage = true;
	storageConfig = { {"shop", 100.f} };
}

FactoryRoom::FactoryRoom() {
	isManufacture = true;
	manufactureTypes = { "experience" };
}
