#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "Containers/Queue.h"

#include <atomic>

#include "ForeverPopulaceFrameworkComponent.generated.h"

class Map;
class Populace;
class Citizen;
class Room;
class ACitizenElement;

// 对应旧Framework Actor `Populace`(C++ Base:PopulaceBase)。这次进入populace域时第一次
// 真正填内容：按玩家距离流式生成/销毁ACitizenElement(老工程APopulaceBase同款做法，和
// ABuildingElement"开局全建好、常驻到关卡结束"不是同一套架构——人口规模通常比建筑数量
// 大得多，流式管理是更保险的默认选择)。职责说明见ForeverPopulaceFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverPopulaceFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverPopulaceFrameworkComponent();

	// 由AForeverFrameworkActor在map->Checkin(*populace)之后调用一次——这次不在这里
	// SpawnActor，只是缓存populace->GetCitizens()列表，真正的生成/销毁全部在
	// TickComponent里按距离做。map生命周期由调用方持有。
	void GenerateCitizens(Map* inMap, Populace* inPopulace);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// PIE停止/退出游戏时置空map——不主动Destroy()场上还活着的ACitizenElement，关卡卸载会
	// 自动清理所有Actor，activeInstances这个TMap本身也会在组件销毁时一起被回收，不需要
	// 手动清空，和UForeverBuildingFrameworkComponent::EndPlay同一个模式。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 按Citizen姓名查找/强制生成对应的ACitizenElement——不看玩家距离，用于"剧情/系统指定
	// 某个具体市民"的场景（如ChangeControlChange，见ForeverStoryFrameworkComponent.cpp）。
	// 已经在activeInstances里直接返回；否则调用SpawnCitizen强制生成一个。找不到同名citizen
	// 或强制生成失败时返回nullptr。
	// @name: 目标citizen姓名（UTF-8转FString比较）
	ACitizenElement* FindOrSpawnCitizenByName(const FString& name);

	// Job按调度产出NPCNavigateChange时，AForeverFrameworkActor::Tick的回调转发到这里：
	// 用destination和市民当前所在room各自的GetNavigationNode()把实际的Dijkstra寻路
	// (map->FindPedestrianPath)派发到线程池异步执行，不在这里(游戏线程)同步算——每天
	// 9点/12点这类所有有工作的市民同一时刻集中触发调度，即使Populace::Tick每帧只处理
	// 一个job timer，Dijkstra本身在这张车道级导航图上单次调用也不便宜，堆在游戏线程上
	// 依然会造成持续的卡顿(PIE验证反馈：从9点持续卡到9点半)。异步结果通过pathResultQueue
	// 传回，TickComponent每帧drain一次，落地逻辑(WalkTo播放真实走路动画/
	// TeleportToRoom瞬移可见Actor/纯瞬移逻辑状态)统一走ApplyWalkResult，和原来同步版本
	// 完全一样，只是触发时机从"RequestWalk调用当下"改成"结果算出来之后的某一帧"。
	// 没有可用导航节点(起点/终点没有对应Node)时没有Dijkstra可算，直接同步走
	// ApplyWalkResult，不需要派发到线程池。
	void RequestWalk(Citizen* citizen, Room* destination);

	// ACitizenElement::WalkTo全部路径点走完后的回调：更新Citizen::SetCurrentRoom。
	void NotifyArrived(Citizen* citizen, Room* destination);

	// 供AForeverFrameworkActor::EndPlay/析构函数在真正delete map/populace之前调用一次：
	// 忙等(短sleep轮询)直到所有已经通过RequestWalk派发到线程池的寻路任务全部执行完(包括
	// 把结果推进pathResultQueue这一步)。必须真的等，不能指望"组件/Actor销毁后台线程自然
	// 停"——后台线程持有的map裸指针在这段等待窗口结束前必须始终有效，用一个和UObject生命
	// 周期无关的static计数器保证，不依赖任何"派发一个GameThread任务、等它跑完"的方案——
	// 后者会在EndPlay这个本身跑在GameThread上的调用栈里死锁(GameThread被卡住等待的
	// 恰好是需要GameThread自己继续跑主循环才能被处理的任务)。pathResultQueue里drain不到
	// 的残留结果(等待期间没有更多TickComponent会跑)随组件一起销毁即可，不需要额外处理——
	// 它们只引用Citizen*/Room*/TArray<FVector>这些纯数据，没有需要手动释放的所有权。
	static void WaitForPendingPathfinding();

private:
	// RequestWalk异步派发到线程池的寻路结果，由TickComponent()每帧drain。只在游戏线程上
	// 被读取(Dequeue)，只在后台线程上被写入(Enqueue)，EQueueMode::Mpsc允许多个后台线程
	// 并发入队，天然匹配"任意时刻可能有好几个citizen的路径同时算完"这个场景。
	struct FPendingWalkResult {
		Citizen* citizen = nullptr;
		Room* destination = nullptr;
		TArray<FVector> waypoints;
	};
	TQueue<FPendingWalkResult, EQueueMode::Mpsc> pathResultQueue;

	// RequestWalk发起异步寻路时+1，对应的后台线程任务把结果推进pathResultQueue之后-1——
	// 全程只用来回答"现在是不是还有后台线程可能在读map"这一个问题，见
	// WaitForPendingPathfinding()。static而不是实例成员：这个组件本身的C++内存在
	// EndPlay/析构期间随时可能被回收，实例成员活不过等待窗口；static存储期从进程启动到
	// 退出全程有效，递减操作发生在纯粹的Core数据(map/citizen/destination指针值)上，不touch
	// 这个UObject实例本身，不存在"递减时对象已经不在了"的问题。
	static std::atomic<int32> pendingPathfindingTasks;

	// RequestWalk/TickComponent共用的路径落地逻辑：existing查一遍activeInstances(可能和
	// RequestWalk发起寻路那一刻已经不是同一个状态——citizen可能被流式销毁/重新生成过，也
	// 可能刚被玩家占有)，路径非空+有Actor就WalkTo；路径为空但有Actor就TeleportToRoom；
	// 否则纯瞬移逻辑状态。和原来同步版本RequestWalk内联的三分支完全一样，这次拆出来给
	// "没有可用导航节点、不需要走线程池"的同步分支和"线程池算完、TickComponent drain"的
	// 异步分支共用。
	void ApplyWalkResult(Citizen* citizen, Room* destination, const TArray<FVector>& waypoints);


	// 生成一个citizen对应的ACitizenElement并登记进activeInstances——从TickComponent的
	// 按距离生成逻辑里提炼出来的公共部分，TickComponent和FindOrSpawnCitizenByName共用。
	// 调用方负责保证citizen不为空、不在activeInstances里。
	// @citizen: 待生成的citizen
	ACitizenElement* SpawnCitizen(Citizen* citizen);

	Map* map = nullptr;

	// GenerateCitizens时从populace->GetCitizens()缓存一份扁平数组，方便按下标做轮询分片
	// (populace自己的std::vector<Citizen*>是Core类型，这里另存一份UE容器方便TArray惯用法，
	// 两者都只是"读"同一批Citizen*，不涉及所有权)。
	TArray<Citizen*> allCitizens;
	TMap<Citizen*, TObjectPtr<ACitizenElement>> activeInstances;
	int32 pollCursor = 0;

	// 每帧只轮询1/streamingBatchStride的citizen，老工程APopulaceBase同款节流思路，避免
	// citizen总数很大时每帧全量距离检测。
	UPROPERTY(EditDefaultsOnly, Category = "Populace")
	int32 streamingBatchStride = 20;

	// 进入这个距离(地图单位)才生成Actor——老工程APopulaceBase同款字面值(2.0)。
	UPROPERTY(EditDefaultsOnly, Category = "Populace")
	float citizenSpawnDistance = 2.f;

	// 超过这个距离才销毁——老工程同款字面值(4.0)，比spawn距离大，形成迟滞区，和building的
	// LOD双阈值同一个防抖动理由。
	UPROPERTY(EditDefaultsOnly, Category = "Populace")
	float citizenDespawnDistance = 4.f;
};
