#include "vehicle_boat.h"

using namespace std;

int BoatVehicle::count = 0;

BoatVehicle::BoatVehicle() : id(count++) {
}

const char* BoatVehicle::GetName() {
	name = "BoatVehicle" + to_string(id);
	return name.data();
}

void BoatVehicle::SetProperty() {
	// 拖网渔船的占位尺寸(UE单位，约25m x 7m)；真实尺寸以蓝图里的骨骼网格为准，不合适再调。
	sizeX = 2500.f; sizeY = 700.f; sizeZ = 800.f;

	// 可驾驶的船：蓝图继承ABoatElement(见Forever/Element/BoatElement.md)，用户在UE里做，路径需和这里一致。
	blueprintPath = "/Game/3rdParty/Trawler/Actor/BP_BoatElement.BP_BoatElement_C";

	// 脚本：只有最基础的"驾驶"选项，没有公交船，见Resource/Story/vehicle_boat.script。
	milestoneNames = { "vehicle_boat" };

	// 下船点(船体局部坐标，船头+X)：船舷左侧(-Y)半宽外再加一点，抬高一些落入水里(玩家落水后自动进入游泳)；
	// 占位估计值，按真实船宽调。
	exitOffsetX = 0.f;
	exitOffsetY = -500.f;
	exitOffsetZ = 300.f;

	drivable = true;
	boardable = false; // 没有航线，不能当乘客搭乘
	capacity = 0;
}
