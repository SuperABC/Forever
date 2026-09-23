#include "Framework/ForeverPopulaceFrameworkComponent.h"

#include "Element/CitizenElement.h"

#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"

#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "map/geometry.h"
#include "populace/populace.h"
#include "populace/citizen.h"

std::atomic<int32> UForeverPopulaceFrameworkComponent::pendingPathfindingTasks{ 0 };

// 和CitizenElement.cpp的CITIZEN_WORLD_SCALE同一个值，这次按本文件既有约定各自维护一份，
// 不额外抽公共头(这个约定详见BuildingElement.cpp顶部注释)。
#define POPULACE_WORLD_SCALE 1000.f
#define POPULACE_HEIGHT_EPSILON 10.f

namespace {
	// 和CitizenElement.cpp里同名函数完全一样的公式——这里只用来估算"还没有Actor的citizen"
	// 当前的逻辑位置，不加随机抖动(抖动是ACitizenElement::Init()真正生成时才加的一次性
	// 效果)。返回false表示这个citizen既没有记录位置、也没有分到房间，无法估算(理论上不会
	// 发生，Map::Checkin()之后每个成功分配住处的citizen都有room/building)。
	bool ComputeLogicalPosition(Citizen* citizen, float& outWorldX, float& outWorldY, float& outWorldZ) {
		if (!citizen) return false;

		if (citizen->HasPosition()) {
			float mapX, mapY, mapZ;
			citizen->GetPosition(mapX, mapY, mapZ);
			outWorldX = mapX * POPULACE_WORLD_SCALE;
			outWorldY = mapY * POPULACE_WORLD_SCALE;
			outWorldZ = mapZ * POPULACE_WORLD_SCALE;
			return true;
		}

		Room* room = citizen->GetCurrentRoom(); // 物理位置，不是家(GetRoom())——两者这次
		// 初始状态重合，但语义上要用当前位置，见citizen.h/room.h的三概念说明
		// building必须从room->GetParentBuilding()反查，不能用citizen->GetBuilding()——
		// 后者是"家"所在的building，进入society域之后currentRoom可能已经通过
		// RequestWalk换成了别的building(比如工作单位)的room，这两个字段不再保证同属一栋
		// 楼，用citizen->GetBuilding()配这里的room会用错的building变换算出完全错误的
		// 世界坐标(PIE验证：市民10点该在商店里，实际用家的building变换算出来的坐标落在
		// 别处，商店里因此看不到任何人)。
		Building* building = room ? room->GetParentBuilding() : nullptr;
		if (!room || !building) return false;

		float localX = room->GetPosX();
		float localY = room->GetPosY();
		float relX = localX - building->GetBodySizeX() * 0.5f + building->GetBodyOffsetX();
		float relY = localY - building->GetBodySizeY() * 0.5f + building->GetBodyOffsetY();
		float rot = building->GetRotation();
		float c = FMath::Cos(rot), s = FMath::Sin(rot);
		outWorldX = (building->GetPosX() + relX * c - relY * s) * POPULACE_WORLD_SCALE;
		outWorldY = (building->GetPosY() + relX * s + relY * c) * POPULACE_WORLD_SCALE;
		outWorldZ = POPULACE_HEIGHT_EPSILON + building->GetFloorBaseZ(room->GetLayer()) * POPULACE_WORLD_SCALE;
		return true;
	}
}

UForeverPopulaceFrameworkComponent::UForeverPopulaceFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = true;
}

void UForeverPopulaceFrameworkComponent::GenerateCitizens(Map* inMap, Populace* inPopulace) {
	map = inMap;
	if (!map || !inPopulace) return;

	allCitizens.Reset();
	for (Citizen* citizen : inPopulace->GetCitizens()) {
		if (citizen) allCitizens.Add(citizen);
	}
}

void UForeverPopulaceFrameworkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	map = nullptr;
	Super::EndPlay(EndPlayReason);
}

ACitizenElement* UForeverPopulaceFrameworkComponent::SpawnCitizen(Citizen* citizen) {
	float worldX, worldY, worldZ;
	if (!ComputeLogicalPosition(citizen, worldX, worldY, worldZ)) return nullptr;

	ACitizenElement* element = GetWorld()->SpawnActor<ACitizenElement>();
	if (element) {
		element->Init(citizen, this);
		activeInstances.Add(citizen, element);
	}
	return element;
}

void UForeverPopulaceFrameworkComponent::RequestWalk(Citizen* citizen, Room* destination) {
	if (!citizen || !destination || !map) return;

	TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen);
	if (existing && *existing && (*existing)->GetController() != nullptr) {
		// 正被玩家占有(这个项目没有AIController，非空Controller只可能是玩家占有)：
		// Job调度这次先整个不生效，被占有的市民不会自己上下班——不管是走路动画还是瞬移
		// 都不做，等玩家取消占有之后下一次调度再正常处理。
		return;
	}

	Room* current = citizen->GetCurrentRoom();
	Node* fromNode = current ? current->GetNavigationNode() : nullptr;
	Node* toNode = destination->GetNavigationNode();

	if (!fromNode || !toNode) {
		// 没有可用导航节点，没有Dijkstra可算，直接同步走落地逻辑(ApplyWalkResult内部按
		// 空路径处理，效果和以前一致)，不需要派发到线程池。
		ApplyWalkResult(citizen, destination, TArray<FVector>());
		return;
	}

	Map* mapPtr = map;
	int32 fromId = fromNode->GetId();
	int32 toId = toNode->GetId();

	// 真正的Dijkstra派发到线程池异步算——每天9点/12点这类所有有工作的市民同一时刻集中
	// 触发调度，单次调用在这张车道级导航图上也不便宜，堆在游戏线程上会造成持续的卡顿
	// (PIE验证反馈：从9点持续卡到9点半，即使Populace::Tick每帧已经只处理一个job
	// timer)。map->FindPedestrianPath是纯读操作(见map.h/map.md)，地图生成完成后
	// pedestrianNavGraph/navAnchorNodes不会再被修改，多个后台线程并发只读安全。
	pendingPathfindingTasks.fetch_add(1, std::memory_order_relaxed);
	TWeakObjectPtr<UForeverPopulaceFrameworkComponent> weakThis(this);
	Async(EAsyncExecution::ThreadPool, [weakThis, mapPtr, fromId, toId, citizen, destination]() {
		std::vector<const Node*> path = mapPtr->FindPedestrianPath(fromId, toId);
		TArray<FVector> waypoints;
		waypoints.Reserve(static_cast<int32>(path.size()));
		for (const Node* node : path) {
			waypoints.Add(FVector(node->GetX(), node->GetY(), node->GetZ()) * POPULACE_WORLD_SCALE);
		}
		// weakThis.Get()在后台线程上解析是UE支持的用法(内部靠序列号判定，不需要跑在
		// 游戏线程上)——只用来判断这个组件是不是已经被销毁，判断结果之后立刻只做入队
		// 这一个动作，不做任何其它UObject操作。就算组件已经销毁、这里什么也不做，也不会
		// 影响下面的计数器递减(计数器是static的，不依赖这个组件实例，见.h的注释)。
		if (UForeverPopulaceFrameworkComponent* component = weakThis.Get()) {
			component->pathResultQueue.Enqueue({ citizen, destination, MoveTemp(waypoints) });
		}
		pendingPathfindingTasks.fetch_sub(1, std::memory_order_release);
		});
}

void UForeverPopulaceFrameworkComponent::ApplyWalkResult(Citizen* citizen, Room* destination,
	const TArray<FVector>& waypoints) {
	if (!citizen || !destination) return;

	// 重新查一次activeInstances——从RequestWalk发起(可能异步经过若干帧)到这里落地之间，
	// citizen完全可能被流式销毁/重新生成过，也可能刚被玩家占有，不能沿用发起那一刻的
	// existing指针/占有状态。
	TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen);
	if (existing && *existing && (*existing)->GetController() != nullptr) {
		return;
	}

	if (waypoints.Num() > 0 && existing && *existing) {
		(*existing)->WalkTo(waypoints, destination);
		return;
	}

	if (existing && *existing) {
		// 有已生成的Actor，但寻路失败(起点/终点没有导航节点，或图不连通)：不能只改
		// Core状态、放着这个可见的Actor不动——那样Citizen逻辑上已经"到家"了，但玩家
		// 眼前的人会一直冻结在原地。直接把Actor也瞬移过去。(用户曾报告过citizen冻结
		// 不动的现象，排查后确认那次的真正原因是CharacterMovementComponent缺
		// bRunPhysicsWithNoController、和这里的寻路失败无关，见CitizenElement.md
		// "已修复的bug"一节——这个分支仍然保留，处理寻路真的失败这一更罕见的情况。)
		(*existing)->TeleportToRoom(destination);
		return;
	}

	// 这个citizen当前没有生成的ACitizenElement(不在流式加载范围内)：直接瞬移逻辑状态，
	// 不模拟中途过程。
	citizen->SetCurrentRoom(destination);
	citizen->ClearPosition();
}

void UForeverPopulaceFrameworkComponent::WaitForPendingPathfinding() {
	while (pendingPathfindingTasks.load(std::memory_order_acquire) > 0) {
		FPlatformProcess::Sleep(0.001f);
	}
}

void UForeverPopulaceFrameworkComponent::NotifyArrived(Citizen* citizen, Room* destination) {
	if (!citizen || !destination) return;
	citizen->SetCurrentRoom(destination);
}

ACitizenElement* UForeverPopulaceFrameworkComponent::FindOrSpawnCitizenByName(const FString& name) {
	for (Citizen* citizen : allCitizens) {
		if (!citizen) continue;
		if (name != UTF8_TO_TCHAR(citizen->GetName().c_str())) continue;

		if (TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen)) {
			if (*existing) return *existing;
			activeInstances.Remove(citizen);
		}
		return SpawnCitizen(citizen);
	}
	return nullptr;
}

void UForeverPopulaceFrameworkComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 把RequestWalk异步派发到线程池、已经算完的寻路结果落地——这一步必须在游戏线程上做
	// (ApplyWalkResult会碰activeInstances/ACitizenElement这些UObject相关状态)，
	// pathResultQueue本身的Dequeue是线程安全的，但落地逻辑不能搬到后台线程去做。
	FPendingWalkResult result;
	while (pathResultQueue.Dequeue(result)) {
		ApplyWalkResult(result.citizen, result.destination, result.waypoints);
	}

	int32 total = allCitizens.Num();
	if (!map || total == 0) return;

	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn) return;
	FVector playerLoc = pawn->GetActorLocation();

	float spawnDistUE = citizenSpawnDistance * POPULACE_WORLD_SCALE;
	float despawnDistUE = citizenDespawnDistance * POPULACE_WORLD_SCALE;
	int32 batchCount = FMath::Clamp(total / FMath::Max(1, streamingBatchStride), 1, total);

	for (int32 i = 0; i < batchCount; i++) {
		int32 idx = (pollCursor + i) % total;
		Citizen* citizen = allCitizens[idx];
		if (!citizen) continue;

		TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen);
		if (!existing) {
			float worldX, worldY, worldZ;
			if (!ComputeLogicalPosition(citizen, worldX, worldY, worldZ)) continue;
			if (FVector::Dist(playerLoc, FVector(worldX, worldY, worldZ)) < spawnDistUE) {
				SpawnCitizen(citizen);
			}
		}
		else {
			ACitizenElement* element = *existing;
			if (!element) {
				activeInstances.Remove(citizen);
				continue;
			}
			if (!element->IsDespawnExempt() && FVector::Dist(playerLoc, element->GetActorLocation()) > despawnDistUE) {
				// 走远到销毁距离之外：先把当前位置转回地图单位写回Citizen(这样下次重新进入
				// 范围直接用这份记录，不再随机抖动一次)，再销毁Actor。
				FVector loc = element->GetActorLocation();
				citizen->SetPosition(loc.X / POPULACE_WORLD_SCALE, loc.Y / POPULACE_WORLD_SCALE,
					loc.Z / POPULACE_WORLD_SCALE);
				element->Destroy();
				activeInstances.Remove(citizen);
			}
		}
	}

	pollCursor = (pollCursor + batchCount) % total;
}
