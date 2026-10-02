#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverTrafficFrameworkComponent.generated.h"

class APlayerController;
class AVehicleElement;
class Map;
class Traffic;

// 载具系统落地：车辆不再按T临时生成/销毁，改成开局时按停车位预置生成(GenerateVehicles)，
// 玩家走近车辆用MeetOption选"上车"（见UForeverStoryFrameworkComponent::ApplyEnterVehicle→
// ApplyEnterVehicle），驾驶时按Q下车（AVehicleElement::ExitVehicle→ExitVehicle，带下车点
// 碰撞检测）。详见ForeverTrafficFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverTrafficFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	// 由AForeverFrameworkActor::EnsureTrafficGenerated()在traffic->Init(map)之后立刻调用一次：
	// 遍历traffic->GetVehicles()里每个已经由Traffic::Init()设好停车位(GetRoom()非空)的
	// Vehicle，换算世界坐标(照抄BuildingElement.cpp::ComputeWorldPosition公式)、
	// LoadClass<AVehicleElement>(vehicle->GetBlueprintPath())、SpawnActor、Init(vehicle, nullptr)
	// （这时候还没人上车，previousPawn先留空）、建好proximity box，存进activeVehicles供
	// ApplyEnterVehicle按名字反查。
	void GenerateVehicles(Map* map, Traffic* traffic);

	// EnterVehicleChange的真正执行：按名字在activeVehicles里找到AVehicleElement，隐藏+
	// despawn-exempt当前pawn、记进vehicleElement的previousPawn，Possess过去。找不到车辆/
	// 当前没有pawn时打一条Warning，不崩溃。
	void ApplyEnterVehicle(const FString& vehicleName, APlayerController* controller);

	// AVehicleElement::ExitVehicle按Q调用：算出候选下车世界坐标(车辆当前transform+
	// VehicleMod的exitOffsetX/Y/Z，按车身当前旋转变换)，用ECC_Pawn通道做胶囊体重叠检测
	// (见[[memory:pawn_preset_ignores_visibility]]，不能用ECC_Visibility)，被挡住就在左上角
	// 打印拒绝提示、不下车；没被挡住就恢复previousPawn（位置/显示/碰撞/Possess），车辆本身
	// 不销毁(这次是预置的真实物件，不是一次性测试对象)。
	void ExitVehicle(APlayerController* controller);

private:
	// Vehicle::GetName()→对应的AVehicleElement，照抄
	// UForeverPopulaceFrameworkComponent::activeInstances的思路，ApplyEnterVehicle按名字
	// 反查用。
	UPROPERTY()
	TMap<FString, TObjectPtr<AVehicleElement>> activeVehicles;
};
