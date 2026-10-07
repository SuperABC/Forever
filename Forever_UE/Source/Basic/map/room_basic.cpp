#include "room_basic.h"

using namespace std;

int ResidenceRoom::count = 0;

ResidenceRoom::ResidenceRoom() : id(count++) {
}

const char* ResidenceRoom::GetName() {
	name = "ResidenceRoom" + to_string(id);
	return name.data();
}

void ResidenceRoom::SetProperty() {
	isResidential = true;
	residentialCapacity = 1;

	// 房间门：随机左右转动的单开门，占位先用项目自己的Cube资产，见door_system_plan.md
	// "用户追加的具体门型要求"一节。只有single/row的门洞带了"room"这个tag才会用到这份
	// spec，见Core/map/door.h的ResolveDoorSpec。
	doorSpecs["room"].mesh = "/Game/Asset/Meshes/Cube.Cube";
	doorSpecs["room"].style = 1; // Swing
	doorSpecs["room"].leaves = 1;
	doorSpecs["room"].randomSide = true;
}

int ShopRoom::count = 0;

ShopRoom::ShopRoom() : id(count++) {
}

const char* ShopRoom::GetName() {
	name = "ShopRoom" + to_string(id);
	return name.data();
}

void ShopRoom::SetProperty() {
	isWorkspace = true;
	workspaceCapacity = 100;

	doorSpecs["room"].mesh = "/Game/Asset/Meshes/Cube.Cube";
	doorSpecs["room"].style = 1; // Swing
	doorSpecs["room"].leaves = 1;
	doorSpecs["room"].randomSide = true;
}

int WarehouseRoom::count = 0;

WarehouseRoom::WarehouseRoom() : id(count++) {
}

const char* WarehouseRoom::GetName() {
	name = "WarehouseRoom" + to_string(id);
	return name.data();
}

void WarehouseRoom::SetProperty() {
	isStorage = true;
	storageConfig = { {"shop", 100.f} };
}

int ParkingRoom::count = 0;

ParkingRoom::ParkingRoom() : id(count++) {
}

const char* ParkingRoom::GetName() {
	name = "ParkingRoom" + to_string(id);
	return name.data();
}

void ParkingRoom::SetProperty() {
	isParking = true;
	parkingSpots = { { {0.5f, 0.f, 0.5f, 0.f}, 0.f } }; // 房间正中心一个车位，朝向跟房间一致
}

int FactoryRoom::count = 0;

FactoryRoom::FactoryRoom() : id(count++) {
}

const char* FactoryRoom::GetName() {
	name = "FactoryRoom" + to_string(id);
	return name.data();
}

void FactoryRoom::SetProperty() {
	isManufacture = true;
	manufactureTypes = { "experience" };
}

int TrainStationRoom::count = 0;

TrainStationRoom::TrainStationRoom() : id(count++) {
}

const char* TrainStationRoom::GetName() {
	name = "TrainStationRoom" + to_string(id);
	return name.data();
}

int AirportRoom::count = 0;

AirportRoom::AirportRoom() : id(count++) {
}

const char* AirportRoom::GetName() {
	name = "AirportRoom" + to_string(id);
	return name.data();
}
