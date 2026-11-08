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
	// 外观：继承AVehicleElement的蓝图类，骨骼网格/轮骨在蓝图里设，这里只存资源路径，见vehicle_basic.md。
	blueprintPath = "/Game/3rdParty/Cars_for_Arcade_Demolition_Racing_Games/Actor/BP_VehicleElement.BP_VehicleElement_C";

	// script_empty纯透传，"靠近弹出上车选项"完全靠milestone触发，见Resource/Story/vehicle_basic.script。
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

	// 路线公交(transitMeshPath)和停车场公交(blueprintPath)共用同一个骨骼网格，见vehicle_basic.md。
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

int TrainVehicle::count = 0;

TrainVehicle::TrainVehicle() : id(count++) {
}

const char* TrainVehicle::GetName() {
	name = "TrainVehicle" + to_string(id);
	return name.data();
}

void TrainVehicle::SetProperty() {
	sizeX = 2134.f; sizeY = 399.f; sizeZ = 577.f; // UE单位，SciFiTram模型的实际包围盒(原点在车身中心附近，底面略低于0)

	// 只有路线版本(静态网格，长边沿X)，没有blueprintPath。
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

	// 模型资产来自Forever_Mod/Test的Plugin，运行时挂载成"/Test/..."包路径，见vehicle_mod.h::transitMeshPath。
	transitMeshPath = "/Test/Airplane/StaticMeshes/airplane.airplane";

	// 模型原始尺寸偏大，缩放到0.15才合适。模型本身是横着建的(正前方不是UE约定的+X)，
	// 需要一个yaw修正，-90度机头朝向才对。
	meshTransform.scale = 0.15f;
	meshTransform.yawOffsetDegrees = -90.f;

	// 停机位飞机用blueprintPath(继承PlaneElement，见PlaneElement.md)，路线飞机用transitMeshPath。
	blueprintPath = "/Game/3rdParty/Low-poly_Airplane/Actor/BP_PlaneElement.BP_PlaneElement_C";

	// 脚本：停机位里的飞机提供"驾驶"选项，线路上的飞机不提供，见Resource/Story/vehicle_plane.script。
	scriptModName = "script_empty";
	milestoneNames = { "vehicle_plane" };

	// 下车点(车身局部坐标)：机身左后方(-X后、-Y左)，避开机翼；是估计值，不合适再调。
	exitOffsetX = -300.f;
	exitOffsetY = -200.f;
	exitOffsetZ = 200.f; // 飞机原点比地面还低，100会让胶囊体底部陷进地形导致永远被挡，抬高后让人落地

	drivable = true;
}
