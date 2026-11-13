#include "vehicle_bus.h"

using namespace std;

int BusVehicle::count = 0;

BusVehicle::BusVehicle() : id(count++) {
}

const char* BusVehicle::GetName() {
	name = "BusVehicle" + to_string(id);
	return name.data();
}

void BusVehicle::SetProperty() {
	sizeX = 786.f; sizeY = 312.f; sizeZ = 347.f; // UE单位，PAZ_652模型的实际包围盒

	// 路线公交(transitMeshPath)和停车场公交(blueprintPath)共用同一个骨骼网格，见vehicle_sedan.md。
	transitMeshPath = "/Game/3rdParty/PAZ_652/StaticMeshes/SKM_paz_652.SKM_paz_652";
	blueprintPath = "/Game/3rdParty/PAZ_652/Actor/BP_BusElement.BP_BusElement_C";

	// 脚本：上下车/驾驶选项，见Resource/Story/vehicle_bus.script。
	scriptModName = "script_empty";
	milestoneNames = { "vehicle_bus" };

	// 下车点(车身局部坐标)：车头+X，乘客从右侧(+Y)靠前的车门下。是估计值，不合适再调。
	exitOffsetX = 250.f;
	exitOffsetY = 250.f;
	exitOffsetZ = 100.f;

	drivable = true;
	boardable = true;
	capacity = 30;
}
