#include "room_shop.h"

ShopRoom::ShopRoom() {
	isWorkspace = true;
	workspaceCapacity = 100;
}

WarehouseRoom::WarehouseRoom() {
	isStorage = true;
	storageConfig = { {"shop", 100.f} };
}
