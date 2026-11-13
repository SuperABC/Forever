#include "room_warehouse.h"

using namespace std;

int WarehouseRoom::count = 0;

WarehouseRoom::WarehouseRoom() : id(count++) {
}

const char* WarehouseRoom::GetName() {
	name = "WarehouseRoom" + to_string(id);
	return name.data();
}

void WarehouseRoom::SetProperty() {
	isStorage = true;
	storageType = "storage_shop";
	// 仓库也有管理员：容纳量1，Organization按容纳量满编定员。
	isWorkspace = true;
	workspaceCapacity = 1;
	rent = 2000;
}
