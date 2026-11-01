#pragma once

#include "CoreMinimal.h"
#include "Controller/VehicleCommandController.h"

#include "BusController.generated.h"

// 公交Controller：在AVehicleCommandController上加"到站停靠+进出站事件"。
// 物理段（以及Controller自己的运动学段）的enter_stop/leave_stop由这里判定并发给公交自己的script
// （见UForeverTrafficFrameworkComponent::FireStopEvent）；远处走Core时刻表的运动学公交（ATransitVehicleElement）
// 不经过这个Controller，事件由Core时刻表发，所以任何时刻事件只有一个发出方。
UCLASS()
class FOREVER_API ABusController : public AVehicleCommandController
{
	GENERATED_BODY()

public:
	ABusController();

	// 每站停靠时长（真实秒）。Core的RouteMod::dwellSeconds是游戏秒，不直接用。
	UPROPERTY(EditAnywhere, Category = "Bus")
	float dwellSeconds = 5.f;

protected:
	virtual void OnStopEntered(int32 nodeId, bool bLoop) override;
	virtual void OnStopLeft(int32 nodeId) override;
	virtual float GetDwellSeconds(int32 nodeId) const override { return dwellSeconds; }
};
