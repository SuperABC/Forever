#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverTrafficFrameworkComponent.generated.h"

class APlayerController;
class AVehicleElement;
class ATransitVehicleElement;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class Map;
class Traffic;

// 载具系统落地：车辆不再按T临时生成/销毁，改成开局时按停车位预置生成(GenerateVehicles)，
// 玩家走近车辆用MeetOption选"上车"（见UForeverStoryFrameworkComponent::ApplyEnterVehicle→
// ApplyEnterVehicle），驾驶时按Q下车（AVehicleElement::ExitVehicle→ExitVehicle，带下车点
// 碰撞检测）。详见ForeverTrafficFrameworkComponent.md。
//
// 这次新增公共交通(大巴/火车/飞机)：GenerateVehicles额外给GetRoute()非空的Vehicle生成
// ATransitVehicleElement(运动学立方体占位，不能上下车)；BuildRouteDebugMesh/BuildTracks
// 是公共交通专属的调试画线/铺轨道，见public_transport_plan.md。
UCLASS()
class FOREVER_API UForeverTrafficFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverTrafficFrameworkComponent();

	// 由AForeverFrameworkActor::EnsureTrafficGenerated()在traffic->Init(map)之后立刻调用一次：
	// 遍历traffic->GetVehicles()，category=="car"(GetRoute()为空)的按原逻辑生成AVehicleElement
	// (照抄BuildingElement.cpp::ComputeWorldPosition公式换算世界坐标、
	// LoadClass<AVehicleElement>(vehicle->GetBlueprintPath())、SpawnActor、Init、建proximity
	// box，存进activeVehicles供ApplyEnterVehicle按名字反查)；GetRoute()非空的(公共交通车辆)
	// 生成ATransitVehicleElement(不需要换算停车位世界坐标，Tick里直接读vehicle->GetTransform()
	// 驱动，初始位置无所谓，存进activeTransitVehicles)。
	void GenerateVehicles(Map* map, Traffic* traffic);

	// EnterVehicleChange的真正执行：按名字在activeVehicles里找到AVehicleElement，隐藏+
	// despawn-exempt当前pawn、记进vehicleElement的previousPawn，Possess过去。找不到车辆/
	// 当前没有pawn时打一条Warning，不崩溃。
	void ApplyEnterVehicle(const FString& vehicleName, APlayerController* controller);

	// AVehicleElement::ExitVehicle按Q调用：算出候选下车世界坐标(车辆当前transform+
	// VehicleMod的exitOffsetX/Y/Z，按车身当前旋转变换)，用ECC_Pawn通道做胶囊体sweep检测
	// (见[[memory:pawn_preset_ignores_visibility]]，不能用ECC_Visibility；也不能用
	// OverlapAnyTestByChannel，建筑/房间/园区边界盒是Trigger档案，重叠类查询会把它们也算成
	// "挡住"，见ExitVehicle实现里的说明)，被挡住就在左上角打印拒绝提示、不下车；没被挡住就
	// 恢复previousPawn（位置/显示/碰撞/Possess），车辆本身不销毁(这次是预置的真实物件，不是
	// 一次性测试对象)。
	void ExitVehicle(APlayerController* controller);

	// 公共交通线路的调试画线：每种stationType一个UProceduralMeshComponent+MID(bus绿/train蓝/
	// plane红/其它默认色)，按route->GetLines()里每条腿的edge几何采样连成ribbon，边中点画一个
	// 箭头指示行驶方向，站点接口画小方块。由bShowRouteDebug控制，关闭时清空已有section而不是
	// 跳过(避免旧可视化残留)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Debug")
	bool bShowRouteDebug = true;

	void BuildRouteDebugMesh(Traffic* traffic);

	// 铺公共交通专属的轨道/跑道mesh——只处理ShouldDrawPath()&&!GetTrackMesh().empty()的线路，
	// 思路同ForeverRoadnetFrameworkComponent的BuildRoadInstances(按弧长重复摆放
	// InstancedStaticMeshComponent)。这次三种内置线路的trackMesh都是空字符串，只打一条Log，
	// 不实际铺设——真正的铺设逻辑留给以后有真实轨道资产的内容类型消费。
	void BuildTracks(Traffic* traffic);

private:
	// Vehicle::GetName()→对应的AVehicleElement，照抄
	// UForeverPopulaceFrameworkComponent::activeInstances的思路，ApplyEnterVehicle按名字
	// 反查用。
	UPROPERTY()
	TMap<FString, TObjectPtr<AVehicleElement>> activeVehicles;

	// Vehicle::GetName()→对应的ATransitVehicleElement——这次公共交通车辆不能上下车，不需要
	// 按名字反查，纯粹持有引用防止被GC，和activeVehicles分开存只是为了类型不同。
	UPROPERTY()
	TMap<FString, TObjectPtr<ATransitVehicleElement>> activeTransitVehicles;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> pureBaseMaterial;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> busRouteMesh;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> trainRouteMesh;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> planeRouteMesh;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> otherRouteMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> busRouteMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> trainRouteMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> planeRouteMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> otherRouteMaterial;

	// 按stationType选调试mesh/MID("bus"/"train"/"plane"，其它一律落到other那一组)，没有
	// pureBaseMaterial(资产缺失)时返回的MID可能是nullptr，调用方自己判空。
	void GetRouteDebugTarget(const FString& stationType, UProceduralMeshComponent*& outMesh,
		UMaterialInstanceDynamic*& outMaterial);
};
