#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"

#include "PlaneWheel.generated.h"

// 飞机鼻轮——参与转向(地面A/D)，不参与手刹/驱动。推力直接作用在机身上(见PlaneElement.md)，
// 轮半径是估值，按模型在蓝图子类里改，见PlaneWheel.md。
UCLASS()
class FOREVER_API UPlaneWheelNose : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UPlaneWheelNose();
};

// 飞机主轮(左右各一)——不转向、不驱动，参与手刹(Space)，用来在地面上刹停/停机位里驻车。
UCLASS()
class FOREVER_API UPlaneWheelMain : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UPlaneWheelMain();
};
