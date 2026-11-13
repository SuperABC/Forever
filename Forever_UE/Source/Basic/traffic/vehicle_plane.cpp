#include "vehicle_plane.h"

using namespace std;

int PlaneVehicle::count = 0;

PlaneVehicle::PlaneVehicle() : id(count++) {
}

const char* PlaneVehicle::GetName() {
	name = "PlaneVehicle" + to_string(id);
	return name.data();
}

void PlaneVehicle::SetProperty() {
	sizeX = 3000.f; sizeY = 2500.f; sizeZ = 800.f;

	// 线路飞机和停机位飞机共用Forever_UE的Low-poly_Airplane模型，见vehicle_mod.h::transitMeshPath。
	transitMeshPath = "/Game/3rdParty/Low-poly_Airplane/StaticMeshes/low_poly_airplane.low_poly_airplane";

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
