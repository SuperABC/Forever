#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverTrafficFrameworkComponent.generated.h"

class APlayerController;

// 阶段4-3:对应旧Framework Actor `Traffic`(C++ Base:TrafficBase)。Route/Station两个域
// 仍然是空实现，这次先落地"上下车"这一件事——玩家按T键（Test动作，
// AForeverCharacter::ToggleVehicle/AVehicleElement::ToggleVehicle都会转调
// RequestToggleVehicle）时，在当前pawn位置生成一辆Vehicle+对应的AVehicleElement并占有它，
// 或者反过来删掉当前占有的车辆、恢复到上车前的pawn，详见ForeverTrafficFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverTrafficFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	// T键共用入口——AForeverCharacter和AVehicleElement都不方便直接互相知道对方，也不是
	// AForeverFrameworkActor的子组件（拿不到GetOwner()那条近路），所以各自的T键处理函数都
	// 调用这个静态方法：按world找场景里唯一的AForeverFrameworkActor，转发给它的
	// TrafficFramework实例调用真正的ToggleVehicle。找不到framework/trafficFramework/
	// controller时什么都不做。
	static void RequestToggleVehicle(UWorld* world, APlayerController* controller);

	// 真正的上下车切换：当前controller->GetPawn()是AVehicleElement就下车（删Vehicle+
	// Actor，把上车前的pawn摆到车辆当前位置再Possess回去）；否则在当前pawn位置生成一辆车
	// 并占有它（隐藏原pawn但不销毁，下车时用得上）。详见.cpp实现。
	void ToggleVehicle(APlayerController* controller);

private:
	// 生成不重复的测试车辆名，如"TestVehicle0"——这一阶段车辆纯粹是临时测试对象，不需要
	// 更有意义的命名规则。
	int32 vehicleCounter = 0;
};
