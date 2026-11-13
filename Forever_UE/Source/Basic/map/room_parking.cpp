#include "room_parking.h"

using namespace std;

int ParkingRoom::count = 0;

ParkingRoom::ParkingRoom() : id(count++) {
}

const char* ParkingRoom::GetName() {
	name = "ParkingRoom" + to_string(id);
	return name.data();
}

void ParkingRoom::SetProperty() {
	isParking = true;
	// 房间正中心一个车位，朝向跟房间一致；种类里的"bus"对应原先InitParkedVehicles里"一半概率换成
	// 可驾驶公交"的临时测试逻辑(现在是数据)，不想要公交就从这里删掉"bus"。
	parkingSpots = { { {0.5f, 0.f, 0.5f, 0.f}, 0.f, { "car", "bus", "truck" } } };
}
