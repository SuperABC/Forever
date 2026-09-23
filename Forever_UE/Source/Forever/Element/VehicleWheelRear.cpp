#include "Element/VehicleWheelRear.h"

UVehicleWheelRear::UVehicleWheelRear()
{
	WheelRadius = 35.f;
	FrictionForceMultiplier = 3.f;
	MaxSteerAngle = 0.f;
	bAffectedBySteering = false;
	bAffectedByHandbrake = true;
	// UChaosVehicleWheel::bAffectedByEngine默认是false(基类构造函数没有设它)，AxleType也
	// 默认是Undefined——不显式设true的话，SetupVehicle()里"AxleType==Undefined时直接用
	// bAffectedByEngine原值"这条分支会让EngineEnabled一直是false，油门给再大力矩也传不到
	// 任何轮子上(实测踩过：车完全不响应油门，其余诊断信息如hasController/numWheels全部
	// 正常，唯独forwardSpeed纹丝不动)。这里配合DifferentialSetup默认的RearWheelDrive，
	// 后轮设true、前轮(VehicleWheelFront)保持false。
	bAffectedByEngine = true;
	SuspensionMaxRaise = 8.f;
	SuspensionMaxDrop = 8.f;
}
