#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"

#include "VehicleWheelRear.generated.h"

// 后轮配置——参与手刹，不参与转向。是否参与驱动由差速器配置(DifferentialSetup)决定，
// 照抄引擎Vehicle模板TP_VehicleAdvWheelRear的最小配置。
UCLASS()
class FOREVER_API UVehicleWheelRear : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UVehicleWheelRear();
};
