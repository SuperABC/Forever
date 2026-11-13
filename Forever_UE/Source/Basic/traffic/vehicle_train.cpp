#include "vehicle_train.h"

using namespace std;

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
