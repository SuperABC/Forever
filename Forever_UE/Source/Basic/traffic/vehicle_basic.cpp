#include "vehicle_basic.h"

using namespace std;

int VehicleBasic::count = 0;

VehicleBasic::VehicleBasic() : id(count++) {
	// 外观：指向一个继承AVehicleElement的蓝图类(Content/Blueprint/Vehicle/
	// BP_VehicleBasic，CarMesh用ChaosModularVehicleExamples插件自带的SKM_SportsCar，
	// 轮子骨骼名Phys_Wheel_FL/FR/BL/BR)，骨骼网格/轮子骨骼名在蓝图编辑器里设置，这里
	// 只存蓝图类的资源路径——见vehicle_mod.h/VehicleElement.md的说明。
	blueprintPath = "/Game/Blueprint/Vehicle/BP_VehicleBasic.BP_VehicleBasic_C";
}

const char* VehicleBasic::GetName() {
	name = "VehicleBasic" + to_string(id);
	return name.data();
}
