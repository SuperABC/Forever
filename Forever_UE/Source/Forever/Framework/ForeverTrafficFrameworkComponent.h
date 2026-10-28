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
class Vehicle;

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

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// 由AForeverFrameworkActor::EnsureTrafficGenerated()在traffic->Init(map)之后立刻调用一次：
	// 遍历traffic->GetVehicles()，category=="car"(GetRoute()为空)的按原逻辑生成AVehicleElement
	// (照抄BuildingElement.cpp::ComputeWorldPosition公式换算世界坐标、
	// LoadClass<AVehicleElement>(vehicle->GetBlueprintPath())、SpawnActor、Init、建proximity
	// box，存进activeVehicles供ApplyEnterVehicle按名字反查)；GetRoute()非空的(公共交通车辆)
	// 生成ATransitVehicleElement(不需要换算停车位世界坐标，Tick里直接读vehicle->GetTransform()
	// 驱动，初始位置无所谓，存进activeTransitVehicles)。
	// 注意：非公共交通的车辆(AVehicleElement)不在这里直接生成，只登记停放位置(carEntries)，由
	// TickComponent按玩家距离流式生成/销毁，和市民(UForeverPopulaceFrameworkComponent)同一个
	// 思路：停着的车是带物理模拟的骨骼网格+Chaos车辆组件+触发盒，全城一次性生成几百辆对游戏
	// 线程负担很重。
	void GenerateVehicles(Map* map, Traffic* traffic);

	// EnterVehicleChange的真正执行：按名字在activeVehicles里找到AVehicleElement，隐藏+
	// despawn-exempt当前pawn、记进vehicleElement的previousPawn，Possess过去。找不到车辆/
	// 当前没有pawn时打一条Warning，不崩溃。
	// bSilent=true时不给车辆script发enter_vehicle事件(读档恢复"正在开的车"：script变量/选项已随存档
	// 恢复，再发一次事件会让script重复处理)。
	void ApplyEnterVehicle(const FString& vehicleName, APlayerController* controller, bool bSilent = false);

	// board_vehicle Change的真正执行：搭乘线路上的公交(ATransitVehicleElement)。和ApplyEnterVehicle
	// 同一套(隐藏原pawn、记previousPawn、Possess)，区别是搭乘者只有鼠标视角、车自己跑。bSilent同上。
	void ApplyBoardVehicle(const FString& vehicleName, APlayerController* controller, bool bSilent = false);

	// leave_vehicle / force_leave Change的真正执行：玩家下车(驾驶和搭乘都行)。vehicleName为空=玩家当前
	// 所在的任何载具，非空=只在玩家正在这辆里才执行。bForce=true(force_leave)时驾驶者也不做遮挡判定。
	void ApplyLeaveVehicle(const FString& vehicleName, APlayerController* controller, bool bForce);

	// 线路上的公交Actor(按Vehicle::GetName()查)，找不到返回nullptr。
	ATransitVehicleElement* FindTransitElement(const FString& vehicleName) const;

	// 这辆车当前在场景里的Actor(停着/私家车；公交车没有交互选项，不在这张表里)，找不到返回nullptr。
	AVehicleElement* FindVehicleElement(const FString& vehicleName) const;

	// 按名字找这辆车的Actor，当前没有生成(离玩家太远)就不看距离强制生成一个——剧情的
	// enter_vehicle/读档恢复"正在开的车"指定的车不一定在玩家附近。找不到这辆车返回nullptr。
	AVehicleElement* FindOrSpawnVehicle(const FString& vehicleName);

	// AVehicleElement::ExitVehicle按Q调用：算出候选下车世界坐标(车辆当前transform+
	// VehicleMod的exitOffsetX/Y/Z，按车身当前旋转变换)，用ECC_Pawn通道做胶囊体sweep检测
	// (见[[memory:pawn_preset_ignores_visibility]]，不能用ECC_Visibility；也不能用
	// OverlapAnyTestByChannel，建筑/房间/园区边界盒是Trigger档案，重叠类查询会把它们也算成
	// "挡住"，见ExitVehicle实现里的说明)，被挡住就在左上角打印拒绝提示、不下车；没被挡住就
	// 恢复previousPawn（位置/显示/碰撞/Possess），车辆本身不销毁(这次是预置的真实物件，不是
	// 一次性测试对象)。
	// 搭乘者(ATransitVehicleElement)也走这条流程，但不做遮挡判定；bForce=true时驾驶者也不做。
	void ExitVehicle(APlayerController* controller, bool bForce = false);

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

	// 车辆流式生成/销毁的距离(地图单位)，是市民(citizenSpawnDistance 2.0/citizenDespawnDistance 4.0)
	// 的两倍；判断距离时Z方向放大5倍(不同楼层的东西一般互相看不到)，和市民同一个度量。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	float vehicleSpawnDistance = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	float vehicleDespawnDistance = 8.f;

	// 每帧最多生成几辆车(车是物理模拟的骨骼网格+Chaos组件，生成成本比市民还高)，超过就等下一帧。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	int32 maxVehicleSpawnsPerFrame = 1;

private:
	// 玩家进出载具(驾驶或搭乘)时给这辆车自己的script发enter_vehicle/leave_vehicle事件。
	void FireVehicleEvent(Vehicle* vehicle, bool entering, const char* mode);

	// 一辆非公共交通车辆的登记项：不管当前有没有生成Actor都存在。location/yaw是它最后一次已知
	// 的位置(没生成时=停车位或存档/上次销毁时的位置；生成着时每帧同步)。不能标UPROPERTY：
	// Vehicle*是Core层类型。
	struct FCarEntry {
		Vehicle* vehicle = nullptr;
		FString name;
		FString blueprintPath;
		FVector location = FVector::ZeroVector;
		float yaw = 0.f;
		bool bBroken = false; // 加载蓝图失败过，不再重试(避免每帧刷警告)
		TWeakObjectPtr<AVehicleElement> element;
	};
	TArray<FCarEntry> carEntries;

	// 按登记项生成Actor并记进activeVehicles，失败返回nullptr。
	AVehicleElement* SpawnCar(FCarEntry& entry);

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
