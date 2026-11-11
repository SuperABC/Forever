#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include "DoorComponent.generated.h"

class Door;
class Citizen;
class USkeletalMesh;
class UDoorAnimInstance;
class UBoxComponent;
class UPrimitiveComponent;
struct FHitResult;

// 门用的骨骼网格组件：包围盒固定为"关门状态的网格盒 + 门扇绕铰链转到任意角度扫过的范围"，不随动画/Physics Asset每帧重算。
// 默认的重算在门扇转开后跟不上姿势，门会被可见性/遮挡剔除成一帧有一帧无；等比放大包围盒则会让包围球过大拖慢阴影/灯光，
// 所以这里按门的形状精确给盒子。盒子是组件局部(缩放前的网格)空间，见CalcBounds。
UCLASS()
class FOREVER_API UDoorMeshComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	void SetDoorLocalBounds(const FBoxSphereBounds& inBounds) { doorLocalBounds = inBounds; }

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override {
		return doorLocalBounds.TransformBy(LocalToWorld);
	}

private:
	FBoxSphereBounds doorLocalBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(50.f), 50.f);
};

// 自动门：骨骼网格门+触发盒+挡板+开合度，绑定Core端Door*，挂在所属对象的渲染Actor上(建筑门/房间门挂ABuildingElement，园区门挂
// UForeverZoneFrameworkComponent的owner)。门禁判定是Door::CanPass()，这里只负责"谁进触发盒→该不该开→开合度插值→写给动画实例"，
// 模型怎么动由动画蓝图决定，实现要点见DoorComponent.md。
UCLASS()
class FOREVER_API UForeverDoorComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UForeverDoorComponent();

	// 绑定Core端Door*，按Door的世界坐标/朝向/外观生成骨骼门/挡板/触发盒。
	void Init(Door* inDoor);
	// 交互选项在运行中变了(add_option/remove_option，由AForeverFrameworkActor::RefreshInteractOptions
	// 在游戏线程上调用)：重新烘焙cachedOptions；玩家正站在旁边时同步替换MeetOption列表里的选项。
	void RefreshOptions();

	// 当前存在的门组件(按Door*查)，供RefreshInteractOptions找到要刷新的那一扇门。门组件跟着
	// 建筑近景LOD创建/销毁，没生成时返回nullptr。
	static UForeverDoorComponent* FindForDoor(const Door* door);

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// 置空door(Tick/Overlap回调都要先判空)。
	void ClearDoor() { door = nullptr; }

private:
	// 骨骼网格门：按门洞宽高对齐包围盒——X、Y等比缩放(系数=门宽/包围盒宽，厚度跟着等比，绕Z的平开不变形)，
	// Z独立缩放到门高；包围盒中心落在(0,0,门高/2)。见door.md"实际摆位"一节。
	void BuildSkeletalDoor(USkeletalMesh* mesh, float doorWidth, float doorHeight);
	// 挡板：门关着时挡人(骨骼网格自己不带碰撞)，开到一半以上放行。
	void BuildBlocker(float doorWidth, float doorHeight);
	void BuildTriggerBox(float doorWidth, float doorHeight);

	// 门从全关开始打开时调用一次，按approacher在门平面哪一侧(组件本地Y的正负)记下OpenSide：
	// 从房间外靠近就往房间内开，从房间内靠近就往外开。门已经开着(或正在关、还没关严)时不调用，
	// 避免门在半空中反向翻转。
	void ChooseOpenSide(AActor* approacher);

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 按OtherActor解出(Citizen*,isPlayer)这对身份——ACitizenElement取GetCitizen()；玩家
	// pawn(含被ChangeControlChange换去操控的任意AForeverCharacter)isPlayer=true；
	// AVehicleElement有人驾驶时用GetPreviousPawn()递归判定，空车返回(nullptr,false)(只开
	// Open的门，见Door::CanPass)；其余(公交载具等)也是(nullptr,false)。
	void ResolvePasserIdentity(AActor* actor, Citizen*& outCitizen, bool& outIsPlayer) const;

	void UpdateOpenState();

	Door* door = nullptr;

	static TMap<const Door*, TWeakObjectPtr<UForeverDoorComponent>> componentsByDoor;

	UPROPERTY()
	TObjectPtr<UDoorMeshComponent> doorMesh;

	UPROPERTY()
	TObjectPtr<UBoxComponent> blocker;

	UPROPERTY()
	TObjectPtr<UBoxComponent> trigger;

	// 当前在触发盒内、且CanPass()通过的Actor集合——非空就该开，清空后delayedCloseSeconds
	// 秒再关，见door.md"开关状态"一节。TWeakObjectPtr防止Actor销毁后变成悬空指针。
	TSet<TWeakObjectPtr<AActor>> passers;
	float closeAtTime = -1.f; // <0表示没有正在倒计时的关门请求
	bool bWantsOpen = false;
	float openProgress = 0.f; // 0=全关，1=全开，Tick()按openSeconds插值趋近bWantsOpen对应的目标
	float openSide = 1.f;     // 见ChooseOpenSide

	// 动画实例的暂停：全关且空闲时，先多Tick一帧把0写给动画实例并让它评估一次，再暂停动画、停Tick。
	bool bAnimPaused = false;
	int32 idleTicks = 0;
	bool bBlockerEnabled = true;
	bool bWarnedAnimClass = false;

	// 这扇门的交互名(烘焙快照，Overlap回调只读这份，不解引用door)。
	FString interactNameOnly;
	TArray<FString> cachedOptions;
};
