#include "Controller/BusController.h"

#include "Element/VehicleElement.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"

ABusController::ABusController()
{
	// 公交又高又宽，弯道限速比默认更保守，防侧翻。
	maxLateralAccel = 200.f;
	maxSpeed = 1400.f;
}

void ABusController::OnStopEntered(int32 nodeId, bool bLoop)
{
	if (!bLoop) return; // 导航到站只是进线路前的路过，不算进站
	AVehicleElement* vehicle = GetDrivenVehicle();
	if (UForeverTrafficFrameworkComponent* owner = GetOwnerFramework()) {
		owner->FireStopEvent(vehicle ? vehicle->GetVehicle() : nullptr, nodeId, true);
	}
}

void ABusController::OnStopLeft(int32 nodeId)
{
	AVehicleElement* vehicle = GetDrivenVehicle();
	if (UForeverTrafficFrameworkComponent* owner = GetOwnerFramework()) {
		owner->FireStopEvent(vehicle ? vehicle->GetVehicle() : nullptr, nodeId, false);
	}
}
