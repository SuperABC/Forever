#include "Element/VehicleWheelFront.h"

UVehicleWheelFront::UVehicleWheelFront()
{
	WheelRadius = 35.f; // 估值，和车身模型的实际轮子半径不一定对应
	FrictionForceMultiplier = 3.f;
	MaxSteerAngle = 40.f;
	bAffectedBySteering = true;
	bAffectedByHandbrake = false;
	// 默认就是false，显式写出来只是不留疑问(后轮驱动，见VehicleWheelRear.cpp)。
	bAffectedByEngine = false;
	SuspensionMaxRaise = 8.f;
	SuspensionMaxDrop = 8.f;
}
