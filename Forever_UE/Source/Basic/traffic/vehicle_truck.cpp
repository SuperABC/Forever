#include "vehicle_truck.h"

using namespace std;

int TruckVehicle::count = 0;

TruckVehicle::TruckVehicle() : id(count++) {
}

const char* TruckVehicle::GetName() {
	name = "TruckVehicle" + to_string(id);
	return name.data();
}

void TruckVehicle::SetProperty() {
	sizeX = 556.f; sizeY = 229.f; sizeZ = 199.f; // UE单位，ZIL-130模型的实际包围盒

	// 驾驶蓝图：从AVehicleElement派生，轮子半径/重心在蓝图里填。
	blueprintPath = "/Game/3rdParty/ZIL_130__body____Lowpoly/Actor/BP_TruckElement.BP_TruckElement_C";

	// 脚本：靠近弹出"上车"，复用小汽车的vehicle_basic。
	scriptModName = "script_empty";
	milestoneNames = { "vehicle_basic" };

	// 下车点(车身局部坐标)：车头+X，驾驶室在前部，从左侧(-Y)车门下。是估计值，不合适再调。
	exitOffsetX = 150.f;
	exitOffsetY = -220.f;
	exitOffsetZ = 100.f;

	drivable = true;
}
