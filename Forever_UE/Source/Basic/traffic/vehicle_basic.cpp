#include "vehicle_basic.h"

using namespace std;

int VehicleBasic::count = 0;

VehicleBasic::VehicleBasic() : id(count++) {
}

const char* VehicleBasic::GetName() {
	name = "VehicleBasic" + to_string(id);
	return name.data();
}

void VehicleBasic::SetProperty() {
	// 外观：指向一个继承AVehicleElement的蓝图类(Content/Blueprint/Vehicle/
	// BP_VehicleBasic，CarMesh用ChaosModularVehicleExamples插件自带的SKM_SportsCar，
	// 轮子骨骼名Phys_Wheel_FL/FR/BL/BR)，骨骼网格/轮子骨骼名在蓝图编辑器里设置，这里
	// 只存蓝图类的资源路径——见vehicle_mod.h/VehicleElement.md的说明。
	blueprintPath = "/Game/Blueprint/Vehicle/BP_VehicleBasic.BP_VehicleBasic_C";

	// "empty"这个ScriptMod id纯粹是透传(AutoCopy(actions)，不做任何占位替换)，这次的
	// "靠近弹出上车选项"完全靠milestone触发就够了，不需要WrapScript介入，见
	// Resource/Story/vehicle_basic.script。
	scriptModName = "empty";
	milestoneNames = { "vehicle_basic" };

	category = "car";
	drivable = true;
}

int BusVehicle::count = 0;

BusVehicle::BusVehicle() : id(count++) {
}

const char* BusVehicle::GetName() {
	name = "BusVehicle" + to_string(id);
	return name.data();
}

void BusVehicle::SetProperty() {
	category = "bus";
	sizeX = 1000.f; sizeY = 300.f; sizeZ = 300.f; // UE单位，一辆公交车大致的长宽高占位比例
}

int TrainVehicle::count = 0;

TrainVehicle::TrainVehicle() : id(count++) {
}

const char* TrainVehicle::GetName() {
	name = "TrainVehicle" + to_string(id);
	return name.data();
}

void TrainVehicle::SetProperty() {
	category = "train";
	sizeX = 2000.f; sizeY = 300.f; sizeZ = 350.f;
}

int PlaneVehicle::count = 0;

PlaneVehicle::PlaneVehicle() : id(count++) {
}

const char* PlaneVehicle::GetName() {
	name = "PlaneVehicle" + to_string(id);
	return name.data();
}

void PlaneVehicle::SetProperty() {
	category = "plane";
	sizeX = 3000.f; sizeY = 2500.f; sizeZ = 800.f;
}
