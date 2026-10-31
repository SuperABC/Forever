#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"

#include "PlaneWheel.generated.h"

// 飞机鼻轮——参与转向(地面A/D)，不参与手刹/驱动。飞机的推力是直接作用在机身上的力(见
// PlaneElement.md)，轮子不带动力，只负责转向、滚动和刹车。轮半径是估值，实际按飞机模型
// 的轮子大小调：在编辑器里基于这个类建一个蓝图子类改WheelRadius，再填到飞机蓝图的WheelSetups里。
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
