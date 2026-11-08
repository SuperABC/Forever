#include "Element/VehicleWheelRear.h"

UVehicleWheelRear::UVehicleWheelRear()
{
	WheelRadius = 35.f;
	FrictionForceMultiplier = 3.f;
	// 引擎默认1500/3000的两倍：载具(尤其卡车)刹车距离太长。
	MaxBrakeTorque = 3000.f;
	MaxHandBrakeTorque = 6000.f;
	MaxSteerAngle = 0.f;
	bAffectedBySteering = false;
	bAffectedByHandbrake = true;
	// 必须显式设true：基类默认false且AxleType为Undefined，不设的话EngineEnabled恒为false，油门传不到任何轮子
	// (车完全不响应油门)。配合DifferentialSetup默认的RearWheelDrive，前轮保持false。见VehicleWheelRear.md。
	bAffectedByEngine = true;
	SuspensionMaxRaise = 8.f;
	SuspensionMaxDrop = 8.f;
}
