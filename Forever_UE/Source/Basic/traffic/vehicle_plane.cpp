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

	// 线路飞机直接用可驾驶飞机蓝图的骨骼网格(缩放/朝向已烘焙进去)，这样两种飞机的缩放和机头朝向一致，
	// meshTransform保持默认(1倍、不修正yaw)。
	transitMeshPath = "/Game/3rdParty/Low-poly_Airplane/StaticMeshes/SKM_low_poly_airplane.SKM_low_poly_airplane";

	// 停机位飞机用blueprintPath(继承PlaneElement，见PlaneElement.md)，路线飞机用transitMeshPath。
	blueprintPath = "/Game/3rdParty/Low-poly_Airplane/Actor/BP_PlaneElement.BP_PlaneElement_C";

	// 脚本：停机位里的飞机提供"驾驶"选项，线路上的飞机停靠跑道中点时提供"上车/下车"，见Resource/Story/vehicle_plane.script。
	scriptModName = "script_empty";
	milestoneNames = { "vehicle_plane" };

	// 下车点(车身局部坐标)：机身左后方(-X后、-Y左)，避开机翼；是估计值，不合适再调。
	exitOffsetX = -300.f;
	exitOffsetY = -200.f;
	exitOffsetZ = 200.f; // 飞机原点比地面还低，100会让胶囊体底部陷进地形导致永远被挡，抬高后让人落地

	drivable = true;
	boardable = true; // 线路上的飞机可搭乘
	capacity = 150;
}
