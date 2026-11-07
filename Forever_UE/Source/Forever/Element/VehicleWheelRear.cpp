#include "Element/VehicleWheelRear.h"

UVehicleWheelRear::UVehicleWheelRear()
{
	WheelRadius = 35.f;
	FrictionForceMultiplier = 3.f;
	MaxSteerAngle = 0.f;
	bAffectedBySteering = false;
	bAffectedByHandbrake = true;
	// 必须显式设true：基类默认false且AxleType为Undefined，不设的话EngineEnabled恒为false，油门传不到任何轮子
	// (车完全不响应油门)。配合DifferentialSetup默认的RearWheelDrive，前轮保持false。见VehicleWheelRear.md。
	bAffectedByEngine = true;
	SuspensionMaxRaise = 8.f;
	SuspensionMaxDrop = 8.f;
}
