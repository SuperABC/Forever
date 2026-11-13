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

	// script_empty纯透传，"靠近弹出上车选项"完全靠milestone触发，见Resource/Story/vehicle_basic.script。
	scriptModName = "script_empty";
	milestoneNames = { "vehicle_basic" };

	drivable = true;
}
