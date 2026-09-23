#include "Element/VehicleWheelFront.h"

UVehicleWheelFront::UVehicleWheelFront()
{
	WheelRadius = 35.f; // 和车身模型的实际轮子半径不一定精确对应，先给个常见小车量级的估值
	FrictionForceMultiplier = 3.f;
	MaxSteerAngle = 40.f;
	bAffectedBySteering = true;
	bAffectedByHandbrake = false;
	// 见VehicleWheelRear.cpp的注释——bAffectedByEngine默认就是false，这里显式写出来只是
	// 为了不留疑问(配合DifferentialSetup默认RearWheelDrive，前轮不参与驱动)。
	bAffectedByEngine = false;
	SuspensionMaxRaise = 8.f;
	SuspensionMaxDrop = 8.f;
}
