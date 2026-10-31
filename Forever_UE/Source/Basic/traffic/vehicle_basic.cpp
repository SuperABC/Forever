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

	// "empty"这个ScriptMod id纯粹是透传(AutoCopy(actions)，不做任何占位替换)，"靠近弹出
	// 上车选项"完全靠milestone触发就够了，不需要WrapScript介入，见
	// Resource/Story/vehicle_basic.script。
	scriptModName = "script_empty";
	milestoneNames = { "vehicle_basic" };

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
	sizeX = 786.f; sizeY = 312.f; sizeZ = 347.f; // UE单位，PAZ_652模型的实际包围盒

	// 路线上的公交(ATransitVehicleElement，运动学显示)和停车场里的公交(AVehicleElement，Chaos物理驾驶)
	// 用同一个模型：transitMeshPath是骨骼网格(车头+X，原点在车身中心地面，轮骨Phys_Wheel_FL/FR/BL/BR)，
	// blueprintPath是从BP_VehicleBasic派生的驾驶蓝图，CarMesh用的也是这个骨骼网格。
	transitMeshPath = "/Game/3rdParty/PAZ_652/StaticMeshes/SKM_paz_652.SKM_paz_652";
	blueprintPath = "/Game/3rdParty/PAZ_652/Actor/BP_VehicleBus.BP_VehicleBus_C";

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

int TrainVehicle::count = 0;

TrainVehicle::TrainVehicle() : id(count++) {
}

const char* TrainVehicle::GetName() {
	name = "TrainVehicle" + to_string(id);
	return name.data();
}

void TrainVehicle::SetProperty() {
	sizeX = 2134.f; sizeY = 399.f; sizeZ = 577.f; // UE单位，SciFiTram模型的实际包围盒(原点在车身中心附近，底面略低于0)

	// 路线上的火车(ATransitVehicleElement，运动学显示)用SciFiTram静态网格(长边沿X，原点在车身中心地面附近)，
	// 只能搭乘不能驾驶，没有停车位版本，所以没有blueprintPath。
	transitMeshPath = "/Game/3rdParty/SciFiTram/StaticMeshes/SciFiTram.SciFiTram";

	// 脚本：停站时的上车/下车选项，见Resource/Story/vehicle_train.script。
	scriptModName = "script_empty";
	milestoneNames = { "vehicle_train" };

	// 下车点(车身局部坐标)：车头+X，乘客从右侧(+Y)车身中段下。是估计值，不合适再调。
	exitOffsetX = 0.f;
	exitOffsetY = 320.f;
	exitOffsetZ = 100.f;

	boardable = true;
	capacity = 80;
}

int PlaneVehicle::count = 0;

PlaneVehicle::PlaneVehicle() : id(count++) {
}

const char* PlaneVehicle::GetName() {
	name = "PlaneVehicle" + to_string(id);
	return name.data();
}

void PlaneVehicle::SetProperty() {
	sizeX = 3000.f; sizeY = 2500.f; sizeZ = 800.f;

	// 测试飞机模型资产，来自Forever_Mod/Test/UE/Test这个独立mod的Plugin(Test)，
	// ForeverModSubsystem运行时挂载成"/Test/..."包路径，见vehicle_mod.h::transitMeshPath。
	transitMeshPath = "/Test/Airplane/StaticMeshes/airplane.airplane";

	// 模型原始尺寸偏大，缩放到0.15才合适。模型本身是横着建的(正前方不是UE约定的+X)，
	// 需要一个yaw修正，-90度机头朝向才对。
	meshTransform.scale = 0.15f;
	meshTransform.yawOffsetDegrees = -90.f;

	// 机场停机位里的飞机(AAirplaneElement，Chaos物理+手写飞行力学，可驾驶)和线路上的飞机
	// (ATransitVehicleElement，运动学显示)共用这个车型，和公交一样：blueprintPath是停机位里那架
	// 可驾驶飞机的蓝图(继承AirplaneElement，骨骼网格/轮骨/飞行系数在蓝图里设，见AirplaneElement.md)，
	// 路线飞机仍然用transitMeshPath。
	blueprintPath = "/Game/3rdParty/Low-poly_Airplane/Actor/BP_VehiclePlane.BP_VehiclePlane_C";

	// 脚本：停机位里的飞机提供"驾驶"选项，线路上的飞机不提供，见Resource/Story/vehicle_plane.script。
	scriptModName = "script_empty";
	milestoneNames = { "vehicle_plane" };

	// 下车点(车身局部坐标)：机身左后方(-X后、-Y左)，避开机翼；是估计值，不合适再调。
	exitOffsetX = -1000.f;
	exitOffsetY = -800.f;
	exitOffsetZ = 100.f;

	drivable = true;
}
