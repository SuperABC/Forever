#pragma once

#include "CoreMinimal.h"
#include "Player/ForeverCharacter.h"

#include "CitizenElement.generated.h"

class ACitizenController;
class UBoxComponent;
class UPrimitiveComponent;
class Citizen;
class Room;
struct FHitResult;
class UForeverPopulaceFrameworkComponent;

// 一个citizen在场景里显形时对应的Actor，基类AForeverCharacter(可被玩家占有)。由UForeverPopulaceFrameworkComponent按距离流式
// Spawn/Destroy，不常驻，详见CitizenElement.md。
UCLASS()
class FOREVER_API ACitizenElement : public AForeverCharacter
{
	GENERATED_BODY()

public:
	ACitizenElement();

	// 由UForeverPopulaceFrameworkComponent::TickComponent在流式判定"该显形"时调用一次：
	// 绑定这个Element对应哪个Citizen，摆放mesh(HasPosition()为true直接读记录位置，否则用
	// 房间中心+随机抖动、并把算出来的位置写回Citizen)，建一个靠近检测碰撞盒。
	void Init(Citizen* inCitizen, UForeverPopulaceFrameworkComponent* inFramework);

	// 置空citizen；销毁自己的AI Controller，并从framework摘掉自己。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 被玩家占有时切MOVE_Walking，放手时切回MOVE_None并让AI Controller重新接管；Input Mapping Context的增删交给Super。
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	// 沿一串世界坐标路径点走过去：往自己的ACitizenController推一条FollowPath指令，走路本身全在Controller里，
	// 见Controller/CitizenController.h。
	void WalkTo(const TArray<FVector>& waypoints, Room* destination);

	// 自己的AI Controller（Init里创建，随Actor销毁）。玩家占有期间它不是GetController()，但对象仍在。
	ACitizenController* GetCitizenController() const { return aiController; }
	UForeverPopulaceFrameworkComponent* GetFramework() const { return framework.Get(); }

	// 把已生成的Actor瞬移到destination房间(落点计算同Init())，并同步Citizen::SetCurrentRoom+SetPosition；
	// 寻路失败(起点/终点没有导航节点，或图不连通)时由UForeverPopulaceFrameworkComponent::RequestWalk调用，否则Core已"到家"而可见的Actor冻结在原地。
	void TeleportToRoom(Room* destination);

	// 上车时标记这个市民暂时不参与距离销毁判定：上车后Actor隐藏在原地，玩家开车走远会被当成"走远的市民"Destroy，
	// 下车时previousPawn就悬空了。下车时再调一次传false解除。
	void SetDespawnExempt(bool exempt) { bDespawnExempt = exempt; }
	bool IsDespawnExempt() const { return bDespawnExempt; }

	// 供UForeverWeaponComponent命中判定用：从命中的Actor反查Citizen*(不持有所有权)。
	Citizen* GetCitizen() const { return citizen; }
	// 交互选项在运行中变了(add_option/remove_option，由AForeverFrameworkActor::RefreshInteractOptions
	// 在游戏线程上调用)：重新烘焙cachedOptions；玩家正站在旁边时同步替换MeetOption列表里的选项。
	void RefreshOptions();
	// 正在WalkTo的目标房间，没在走路时为nullptr。存档时用来把走路中的市民记成"已经到了"。
	Room* GetWalkDestination() const;

protected:
	virtual void Tick(float DeltaTime) override;

private:
	void BuildProximityBox();

	// 玩家放手后（UnPossessed）下一帧让自己的ACitizenController重新Possess。
	void RepossessAIController();

	UPROPERTY()
	TObjectPtr<ACitizenController> aiController;

	// Init()首次摆放分支、TeleportToRoom()共用的落地位置计算(房间中心+随机抖动+楼层高度)。
	// room为空或反查不到parentBuilding时返回false，调用方保留原坐标。
	bool ComputeRoomLandingSpot(Room* room, float& outWorldX, float& outWorldY, float& outWorldZ);

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	Citizen* citizen = nullptr;
	TWeakObjectPtr<UForeverPopulaceFrameworkComponent> framework;

	bool bDespawnExempt = false;

	UPROPERTY()
	TObjectPtr<UBoxComponent> proximityBox; // 玩家靠近检测——和流式生成/销毁的距离判定是两回事

	// 姓名，供MeetOption UI用(AddOption/RemoveName)，同样是烘焙快照。
	FString citizenNameOnly;

	// citizen->GetOptions()的快照，在BuildProximityBox()里烘焙，Overlap回调只读这份，不解引用citizen。
	TArray<FString> cachedOptions;
};
