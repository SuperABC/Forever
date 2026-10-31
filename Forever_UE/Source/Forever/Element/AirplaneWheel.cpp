#include "Element/AirplaneWheel.h"

UAirplaneWheelNose::UAirplaneWheelNose()
{
	WheelRadius = 25.f;
	FrictionForceMultiplier = 3.f;
	MaxSteerAngle = 40.f;
	bAffectedBySteering = true;
	bAffectedByBrake = false;
	bAffectedByHandbrake = false;
	bAffectedByEngine = false;
	SuspensionMaxRaise = 8.f;
	SuspensionMaxDrop = 8.f;
	SuspensionDampingRatio = 1.2f; // 过阻尼，落地不回弹
}

UAirplaneWheelMain::UAirplaneWheelMain()
{
	WheelRadius = 30.f;
	FrictionForceMultiplier = 3.f;
	MaxSteerAngle = 0.f;
	bAffectedBySteering = false;
	bAffectedByBrake = true;
	bAffectedByHandbrake = true;
	bAffectedByEngine = false;
	SuspensionMaxRaise = 8.f;
	SuspensionMaxDrop = 8.f;
	SuspensionDampingRatio = 1.2f; // 过阻尼，落地不回弹
}
