#include "vehicle_sedan.h"

using namespace std;

int SedanVehicle::count = 0;

SedanVehicle::SedanVehicle() : id(count++) {
}

const char* SedanVehicle::GetName() {
	name = "SedanVehicle" + to_string(id);
	return name.data();
}

void SedanVehicle::SetProperty() {
	// 外观：继承AVehicleElement的蓝图类，骨骼网格/轮骨在蓝图里设，这里只存资源路径，见vehicle_sedan.md。
	blueprintPath = "/Game/3rdParty/Cars_for_Arcade_Demolition_Racing_Games/Actor/BP_VehicleElement.BP_VehicleElement_C";

	// 没有ScriptMod，"靠近弹出上车选项"完全靠milestone触发，见Resource/Story/vehicle_basic.script。
	milestoneNames = { "vehicle_basic" };

	drivable = true;

	// 车身尺寸(UE单位cm)：sizeX=车长，Core据此算倒车出库距离(2倍车长)，见vehicle.md"车位"。占位值，按蓝图网格实际包围盒调。
	sizeX = 450.f;
	sizeY = 180.f;
	sizeZ = 150.f;
}
