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

// 按玩家距离流式生成/销毁ACitizenElement，并负责市民走路(异步寻路)、瞬移、存档同步。
// 设计说明见ForeverPopulaceFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverPopulaceFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverPopulaceFrameworkComponent();

	// 由AForeverFrameworkActor在map->Checkin(*populace)之后调用一次：只缓存市民列表，不SpawnActor，
	// 真正的生成/销毁在TickComponent里按距离做。map生命周期由调用方持有。
	void GenerateCitizens(Map* inMap, Populace* inPopulace);

	// 运行中新增的市民(spawn_npc)加进流式生成列表，之后和其他市民一样按距离生成/销毁Actor。
	void AddCitizen(Citizen* citizen);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// 只置空map，不主动Destroy场上的ACitizenElement(关卡卸载会清理)。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 按Citizen姓名查找/强制生成对应的ACitizenElement，不看玩家距离(剧情指定某个市民的场景)。
	// 找不到同名(或已死亡)citizen或生成失败时返回nullptr。
	// @name: 目标citizen姓名（UTF-8转FString比较）
	ACitizenElement* FindOrSpawnCitizenByName(const FString& name);

	// 让市民走向destination(Job的NPCNavigateChange转发到这里)。寻路派发到线程池异步算，结果经
	// pathResultQueue由TickComponent落地(ApplyWalkResult)；没有可用导航节点时直接同步落地。
	// 在途的市民从当前位置起算。流程与线程安全见.md。
	void RequestWalk(Citizen* citizen, Room* destination);

	// npc_teleport：立刻把市民放到destination。有Actor就把Actor瞬移过去(取消正在走的路)，没有就只改
	// 逻辑位置，等进入流式范围时在那里生成。被玩家占有的市民不动，和RequestWalk一样。
	void TeleportCitizen(Citizen* citizen, Room* destination);

	// 跳时间(jump_time)期间：RequestWalk不再异步寻路，直接TeleportCitizen到目的地(同步到达，npc_arrive照常触发)。
	// Begin把全部在途的行走(异步寻路结果、Core推算中的、Actor正在走的)冲刷到各自的目的地，再开启这个模式；
	// 玩家占有的市民不动。End关闭模式。
	void BeginTimeSkip();
	void EndTimeSkip() { bTimeSkipping = false; }

	// ACitizenController的FollowPath指令走完后的回调：更新Citizen::SetCurrentRoom。
	void NotifyArrived(Citizen* citizen, Room* destination);

	// 市民的currentRoom换成了新房间(previousRoom是换之前的)：房间真的变了就给它自己的
	// Scheduler/Job脚本发npc_arrive。只在真正到达的地方调用(走完路、瞬移、寻路失败落地)，
	// 存档用的SyncPositionsToCore/RestoreAfterSave不发。
	void ReportArrival(Citizen* citizen, Room* previousRoom);

	// ACitizenController的GoToRoom指令用：规划from（世界坐标）到destination房间的行人路径点（世界坐标）。
	// 起点按citizen当前房间/from位置（Map::PlanPedestrianRoute），成功且路径非空返回true。
	bool PlanWalkPath(Citizen* citizen, Room* destination, const FVector& from, TArray<FVector>& outWaypoints) const;

	// script的test_navigate_citizen Change（"你在去哪"选项）：用橙色折线（复用交通组件的那条线）显示这个
	// 市民正在走的路线——当前位置 + 剩余路径点，折线一直显示到这条路走完。不改变市民的行动。市民必须已有Actor
	// （玩家对话时必然在身边）；没在走路时只在屏幕左上角提示。
	void StartTestNavigation(const FString& citizenName);

	// ACitizenElement::EndPlay回调：Actor以任何方式结束（流式销毁、掉出世界被引擎销毁等）都从activeInstances摘掉
	// （那个TMap的Citizen*键没法UPROPERTY，GC不会置空，Actor被引擎销毁后会留下悬空指针）。
	void HandleElementEndPlay(Citizen* citizen, ACitizenElement* element);

	// citizen死亡或离城后调用：Destroy对应Actor并摘除，之后流式生成和FindOrSpawnCitizenByName都跳过它。
	// @citizen: 已经确认!IsActive()的citizen（死亡先调TakeDamage()判定，离城由Emigration置位）。
	void HandleCitizenDeath(Citizen* citizen);

	// 供AForeverFrameworkActor::EndPlay/析构在delete map/populace之前调用：取消排队中的寻路任务，
	// 并忙等到已开始的任务全部结束(后台线程持有map裸指针)。原理见.md。
	static void WaitForPendingPathfinding();

	// 这个市民当前在场景里的Actor(没生成时返回nullptr，不会触发生成)。
	ACitizenElement* FindActiveCitizen(Citizen* citizen) const;

	// 遍历当前已生成出Actor的所有人物(跳过已失效的)——识别模式用它枚举附近的人。
	void ForEachActiveCitizen(TFunctionRef<void(ACitizenElement*)> visitor) const;

	// 存档前调用：把场景里每个市民Actor的位置写回Core的Citizen，并记下"是否已实例化"(Citizen::SetWasSpawned)。
	// 正在走路的市民存即时状态：Actor在走的，把剩余路径(首点=当前位置)临时交给Core行进模拟；Core在推算的，
	// 改写成以当前位置为起点的剩余路径。存档读回后沿同一条路继续走。临时交给Core的那些存完必须调
	// RestoreAfterSave还原，不影响正在进行的游戏。已经算完、还没落地的寻路结果先落地，一起算作"在走路"。
	struct FWalkingSnapshot {
		Citizen* citizen = nullptr;
		Room* currentRoom = nullptr;
		bool hasPosition = false;
		bool bCoreWalk = false; // 存档用的Core行进是临时的，RestoreAfterSave要EndWalk
		float x = 0.f, y = 0.f, z = 0.f;
	};
	TArray<FWalkingSnapshot> SyncPositionsToCore();
	static void RestoreAfterSave(const TArray<FWalkingSnapshot>& walking);

private:
	// RequestWalk异步寻路的结果，TickComponent每帧drain：游戏线程读，多个后台线程写(Mpsc)。
	struct FPendingWalkResult {
		Citizen* citizen = nullptr;
		Room* destination = nullptr;
		TArray<FVector> waypoints;
		uint32 serial = 0; // 发起时的请求序号，落地时和latestWalkRequest比，不是最新的说明已被新导航/瞬移取代
	};
	TQueue<FPendingWalkResult, EQueueMode::Mpsc> pathResultQueue;

	// 每个市民最近一次RequestWalk的序号（TeleportCitizen也会+1来作废在途的寻路）。只在游戏线程读写。
	// 异步寻路期间又来了新导航时，旧的结果晚到也不能落地，否则会把新路线覆盖掉。
	TMap<Citizen*, uint32> latestWalkRequest;

	// 把已算完的寻路结果落地（跳过已被取代的）。TickComponent和SyncPositionsToCore共用。
	void DrainPathResults();

	// 市民正在路上（Actor在走FollowPath，或Core在推算）时取当前脚底世界坐标；不在路上返回false。
	bool GetTransitPosition(Citizen* citizen, FVector& outFeet) const;

	// 已派发未算完的寻路任务数：回答"是否还有后台线程可能在读map"，见WaitForPendingPathfinding()。
	// 必须是static：组件在EndPlay/析构期间随时可能被回收，实例成员活不过等待窗口。
	static std::atomic<int32> pendingPathfindingTasks;
	// WaitForPendingPathfinding置位：还没开始跑的寻路任务直接跳过（只递减计数器），让关闭不用等完整个队列。
	static std::atomic<bool> bCancelPathfinding;

	// 路径落地(同步分支和异步drain共用)：按落地这一刻的状态重新查activeInstances/占有状态，
	// 分流为WalkTo / TeleportToRoom / Core推算(BeginWalk) / 纯瞬移，细节见.md。
	void ApplyWalkResult(Citizen* citizen, Room* destination, const TArray<FVector>& waypoints);



	// 读档后存档里标记为"已实例化"的市民，TickComponent先于流式扫描把它们全部生成出来(不看距离、
	// 不受数量上限限制)，保证看到的人不比存档前少。每帧最多maxSpawnsPerFrame个，生成完才恢复流式扫描。
	TArray<Citizen*> pendingRestoreSpawns;

	// 刚生成了Actor的市民如果在Core里行进，把剩余路径交给Actor接着走(流式生成和读档恢复共用)。
	void HandOffWalkToElement(Citizen* citizen, ACitizenElement* element, double now);

	// 生成citizen对应的ACitizenElement并登记进activeInstances(TickComponent和FindOrSpawnCitizenByName共用)。
	// 调用方负责保证citizen不为空、不在activeInstances里。
	// @citizen: 待生成的citizen
	ACitizenElement* SpawnCitizen(Citizen* citizen);

	Map* map = nullptr;

	// GenerateCitizens时缓存的扁平数组，用于按下标轮询分片；Citizen*不涉及所有权。
	TArray<Citizen*> allCitizens;
	TMap<Citizen*, TObjectPtr<ACitizenElement>> activeInstances;
	TMap<Citizen*, double> spawnTimes; // 最近一次生成的世界时间，minCitizenLifetime内不允许流式销毁

	// 市民生成后至少存活这么多秒才允许流式销毁(刚生成就销毁可能碰上物理/组件还没初始化完)。
	UPROPERTY(EditDefaultsOnly, Category = "Populace")
	float minCitizenLifetime = 2.f;
	int32 pollCursor = 0;

	bool bTimeSkipping = false;

	// 非故事市民每趟行走参与实例化的概率(全局)：初始1，扫描中记录进入生成距离的行走者数峰值x，
	// 取1.5*maxMovingCitizens/x，只减不增。故事市民在Citizen上单独设成1，不受它影响。
	float walkInstantiateProbability = 1.f;
	int32 peakWalkersInRange = 0;
	int32 sweepWalkersInRange = 0;

	// 每帧只轮询1/streamingBatchStride的citizen，避免每帧全量距离检测。
	UPROPERTY(EditDefaultsOnly, Category = "Populace")
	int32 streamingBatchStride = 20;

	// 每帧最多生成几个市民Actor：一次SpawnActor约5ms，连生成几个会出现单帧尖峰。
	UPROPERTY(EditDefaultsOnly, Category = "Populace")
	int32 maxSpawnsPerFrame = 1;

	// 进入这个距离(地图单位)才生成Actor。
	UPROPERTY(EditDefaultsOnly, Category = "Populace")
	float citizenSpawnDistance = 2.f;

	// 超过这个距离才销毁；比spawn距离大形成迟滞区，避免临界距离来回生成/销毁。
	UPROPERTY(EditDefaultsOnly, Category = "Populace")
	float citizenDespawnDistance = 4.f;

	// 同时存在的市民Actor数量上限，静止的和移动中的分开算(免销毁的不算)。
	UPROPERTY(EditDefaultsOnly, Category = "Populace")
	int32 maxStationaryCitizens = 16;

	UPROPERTY(EditDefaultsOnly, Category = "Populace")
	int32 maxMovingCitizens = 16;

	// 移动中的市民生成/销毁距离是静止的这么多倍。
	UPROPERTY(EditDefaultsOnly, Category = "Populace")
	float movingDistanceScale = 2.f;

	// 在屏幕左上角实时显示寻路队列长度（调试用）。
	UPROPERTY(EditDefaultsOnly, Category = "Populace")
	bool bShowPathfindingQueue = true;

};
