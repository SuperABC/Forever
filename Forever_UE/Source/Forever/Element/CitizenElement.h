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
// 基类是AForeverCharacter，不是纯ACharacter——市民要能被玩家真正占有操控（由剧情脚本
// 触发的ChangeControlChange切换，见ForeverStoryFrameworkComponent.md），AForeverCharacter
// 已经有摄像机/移动/Enhanced Input绑定+PossessedBy/UnPossessed这一整套东西，直接继承复用，
// 不需要另起一套。这次仍然默认不能移动（CharacterMovementComponent这次显式设成MOVE_None，
// 见.cpp构造函数），只有被占有时才切换成MOVE_Walking（见PossessedBy覆写）。
//
// T键（Test动作）目前指向"上下车切换"（见ForeverTrafficFrameworkComponent.h），不再调用
// 下面的LogNearbyRelationships()——这个静态方法本身没有删除（还是一个能用的调试工具），
// 只是暂时没有任何按键绑定它。
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

	// 把"当前被占有对象附近的市民名单"(nearbyCitizens)里每一个仍然有效的市民，各自的
	// acquaintances(熟人关系强度)+experiences(四类人际关系历史记录)完整输出到log(UE_LOG，
	// 不是屏幕调试消息)。上一版T键(Test动作)绑定过这个方法，这次T键改指向"上下车切换"（见
	// ForeverTrafficFrameworkComponent.h），没有任何按键绑定这个方法，仍然保留供以后调试用。
	static void LogNearbyRelationships();

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
	// （用户报告过"市民到点该走了，但眼前这个人一直没动过"，但排查后确认那次的真正
	// 原因是ACitizenElement构造函数缺一个`bRunPhysicsWithNoController`开关，不是这里
	// 说的寻路失败——见CitizenElement.md"已修复的bug"一节。这个分支仍然保留，处理
	// 寻路真的失败(起点/终点没有导航节点，或图不连通)这一确实存在但更罕见的情况）。
	void TeleportToRoom(Room* destination);

	// 上车时(UForeverTrafficFrameworkComponent::ToggleVehicle)标记这个市民暂时不参与
	// UForeverPopulaceFrameworkComponent::TickComponent的距离销毁判定——上车后这个Actor
	// 会被隐藏在原地，玩家开车远离之后按正常距离判定会被当成"走远的市民"直接Destroy掉，
	// 等玩家下车时previousPawn就成了悬空指针，表现为"车消失了但人没出来、操控彻底失灵"
	// （用户实测反馈的bug，见ForeverTrafficFrameworkComponent.md）。下车时
	// ToggleVehicle再调一次传false解除标记，恢复正常的距离流式管理。
	void SetDespawnExempt(bool exempt) { bDespawnExempt = exempt; }
	bool IsDespawnExempt() const { return bDespawnExempt; }

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

	bool bDespawnExempt = false;

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
