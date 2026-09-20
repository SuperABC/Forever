#pragma once

#include "CoreMinimal.h"
#include "Player/ForeverCharacter.h"

#include "CitizenElement.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class Citizen;
class Room;
struct FHitResult;
class UForeverPopulaceFrameworkComponent;

// 一个citizen在场景里显形时对应的Actor——和ABuildingElement同一套"每个Core实例对应一个
// 强类型Actor子类"模式（Actor自己持有指向Core对象的裸指针+指向共享框架组件的
// TWeakObjectPtr），区别只有一点：ABuildingElement开局为每栋building常驻生成一个，
// ACitizenElement按玩家距离由UForeverPopulaceFrameworkComponent::TickComponent流式
// SpawnActor/Destroy，不常驻，详见Source/Forever/Element/CitizenElement.md。
//
// 基类是AForeverCharacter，不是纯ACharacter——市民要能被玩家真正占有操控（走近后按T切换，
// 见GetFirstNearby），AForeverCharacter已经有摄像机/移动/Enhanced
// Input绑定+PossessedBy/UnPossessed这一整套东西，直接继承复用，不需要另起一套。这次仍然
// 默认不能移动（CharacterMovementComponent这次显式设成MOVE_None，见.cpp构造函数），只有
// 被占有时才切换成MOVE_Walking（见PossessedBy覆写）。
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

	// PIE停止/退出游戏时置空citizen——和ABuildingElement::EndPlay同一套安全原则：只保证
	// "自己的EndPlay一跑完，自己不会再解引用citizen"，不需要关心和框架Actor/其它Element
	// 的EndPlay谁先谁后。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 被占有时切换成能走动(MOVE_Walking)，取消占有时切回MOVE_None（不然会被
	// CharacterMovementComponent的重力/地面检测挪走）。Input Mapping Context的增删交给
	// Super（AForeverCharacter::PossessedBy/UnPossessed），见
	// [[memory:possession_driven_input_context]]。
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	// 返回"当前被占有对象附近的市民名单"里第一个仍然有效的市民（顺带清理已失效的弱引用），
	// 名单为空则返回nullptr——供AForeverCharacter::SwitchControlledCitizen（T键）使用。
	static ACitizenElement* GetFirstNearby();

	// 调试用：把nearbyCitizens当前的完整内容（含下标、每个人的姓名，失效弱引用标"已失效"，
	// 不做清理）打印到屏幕左上角——排查"按T有时候切换失败"具体每次名单里是什么状况。
	// AForeverCharacter::SwitchControlledCitizen每次按T都调用一次。
	static void DebugPrintNearby();

	// 沿一串世界坐标路径点真正走过去（Job按调度产出NPCNavigateChange时，
	// UForeverPopulaceFrameworkComponent::RequestWalk发现这个citizen当前有对应Actor就调
	// 这个方法）：打开Tick+切MOVE_Walking，每帧朝下一个路径点AddMovementInput，到达即
	// 前进到下一个，全部走完切回MOVE_None+关Tick+回调
	// framework->NotifyArrived(citizen, destination)更新Citizen::SetCurrentRoom。不用
	// AIController/NavMesh——路径点已经由Map::FindPedestrianPath算好，这里只是沿现成
	// 路径点插值前进。
	void WalkTo(const TArray<FVector>& waypoints, Room* destination);

	// 直接把这个已经生成的Actor瞬移到destination房间（房间中心+和Init()同一套随机抖动/
	// 落地高度计算），并同步更新Citizen::SetCurrentRoom+SetPosition——
	// UForeverPopulaceFrameworkComponent::RequestWalk发现这个citizen当前有对应Actor、
	// 但寻路失败（起点/终点没有导航节点，或图不连通）时调用这个方法。不这么做的话，Core
	// 侧的Citizen::SetCurrentRoom已经改成新房间了，但这个可见的Actor会一直冻结在原地不动
	// （PIE验证复现过这个bug："市民到点该走了，但眼前这个人一直没动过"）。
	void TeleportToRoom(Room* destination);

protected:
	virtual void Tick(float DeltaTime) override;

private:
	void BuildProximityBox();

	// Init()"换房间后从未在场景里实例化过"分支、TeleportToRoom()共用的落地位置计算：
	// 房间中心+随机抖动+楼层高度换算成落脚点世界坐标，见Init()原本的注释。room为空
	// 或反查不到parentBuilding时返回false，调用方保留原有worldX/Y/Z不变。
	bool ComputeRoomLandingSpot(Room* room, float& outWorldX, float& outWorldY, float& outWorldZ);

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	Citizen* citizen = nullptr;
	TWeakObjectPtr<UForeverPopulaceFrameworkComponent> framework;

	UPROPERTY()
	TObjectPtr<UBoxComponent> proximityBox; // 玩家靠近检测——和流式生成/销毁的距离判定是两回事

	FString collisionLabel; // Overlap回调只读这份烘焙好的字符串，绝不解引用citizen

	// 当前被占有对象（可能是最初的ADefaultPawn/AForeverCharacter，也可能是另一个citizen）
	// 附近的市民名单，OnOverlapBegin/OnOverlapEnd维护。用TWeakObjectPtr而不是裸指针——
	// 市民会被UForeverPopulaceFrameworkComponent按距离动态Destroy。
	static TArray<TWeakObjectPtr<ACitizenElement>> nearbyCitizens;

	// WalkTo正在走的路径点队列+当前索引，Tick()里消费；走完清空。
	TArray<FVector> pendingWaypoints;
	int32 waypointIndex = 0;

	// WalkTo的目标room，全部路径点走完后回调framework->NotifyArrived(citizen, destination)
	// 时用——不持有生命周期。
	Room* walkDestination = nullptr;
};
