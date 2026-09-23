#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"

#include "VehicleWheelFront.generated.h"

// 前轮配置——参与转向，不参与手刹。是否参与驱动由差速器配置(DifferentialSetup)决定，
// 这里不覆盖bAffectedByEngine，照抄引擎Vehicle模板TP_VehicleAdvWheelFront的最小配置。
UCLASS()
class FOREVER_API UVehicleWheelFront : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UVehicleWheelFront();
};
