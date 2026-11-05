#include "Framework/ForeverPopulaceFrameworkComponent.h"

#include "Element/CitizenElement.h"
#include "Controller/CitizenController.h"

#include "Kismet/GameplayStatics.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"
#include "Framework/ForeverStoryFrameworkComponent.h"
#include "Engine/Engine.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"

#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "map/general_geometry.h"
#include "populace/populace.h"
#include "populace/citizen.h"

std::atomic<int32> UForeverPopulaceFrameworkComponent::pendingPathfindingTasks{ 0 };
std::atomic<bool> UForeverPopulaceFrameworkComponent::bCancelPathfinding{ false };

// 和CitizenElement.cpp的CITIZEN_WORLD_SCALE同一个值，这次按本文件既有约定各自维护一份，
// 不额外抽公共头(这个约定详见BuildingElement.cpp顶部注释)。
#define POPULACE_WORLD_SCALE 1000.f
#define POPULACE_HEIGHT_EPSILON 10.f

// 流式生成/销毁判断距离时Z方向的权重：不同楼层的人一般互相看不到，Z差放大5倍再算距离，
// 头顶/脚下楼层的市民就不会因为三维距离近而被生成。生成和销毁两个阈值用同一个度量，迟滞区间才一致。
#define POPULACE_VERTICAL_DISTANCE_WEIGHT 5.f

namespace {
	// 没有生成Actor的市民(Core按时间推算位置)走得比Actor快的倍数，省得大批通勤市民要很久才到。
	// 生成Actor时按推算到的位置接着走，之后用正常速度，所以进度会被拖慢。
	constexpr float kUnspawnedWalkSpeedScale = 10.f;

	float StreamingDistance(const FVector& a, const FVector& b) {
		FVector delta = a - b;
		delta.Z *= POPULACE_VERTICAL_DISTANCE_WEIGHT;
		return delta.Size();
	}

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
	bCancelPathfinding.store(false, std::memory_order_release); // 新一局（PIE可能连续开）
	map = inMap;
	if (!map || !inPopulace) return;

	allCitizens.Reset();
	for (Citizen* citizen : inPopulace->GetCitizens()) {
		if (citizen) allCitizens.Add(citizen);
	}
}

void UForeverPopulaceFrameworkComponent::AddCitizen(Citizen* citizen) {
	if (citizen) allCitizens.AddUnique(citizen);
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
	if (citizen->IsDead()) return; // 死人不再被调度，见Citizen::Die

	TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen);
	if (existing && *existing && (*existing)->IsPlayerControlled()) {
		// 正被玩家占有(市民自带的ACitizenController不算，只看玩家Controller)：
		// Job调度这次先整个不生效，被占有的市民不会自己上下班——不管是走路动画还是瞬移
		// 都不做，等玩家取消占有之后下一次调度再正常处理。
		return;
	}

	// 新的一次导航：在途的旧寻路结果（晚到的）作废。
	const uint32 serial = ++latestWalkRequest.FindOrAdd(citizen);

	// 正在路上（currentRoom是空的）：从当前实际位置起算，新路线立刻取代旧路线；
	// 不在路上：从当前房间的导航节点起算，已经在目的地房间就什么也不用做。
	FVector transitFeet;
	const bool bInTransit = GetTransitPosition(citizen, transitFeet);
	Room* current = citizen->GetCurrentRoom();
	if (!bInTransit && current == destination) return;

	Node* fromNode = current ? current->GetNavigationNode() : nullptr;
	Node* toNode = destination->GetNavigationNode();

	if (!toNode || (!bInTransit && !fromNode)) {
		// 没有可用导航节点，没有Dijkstra可算，直接同步走落地逻辑(ApplyWalkResult内部按
		// 空路径处理，效果和以前一致)，不需要派发到线程池。
		ApplyWalkResult(citizen, destination, TArray<FVector>());
		return;
	}

	Map* mapPtr = map;
	int32 fromId = fromNode ? fromNode->GetId() : -1;
	int32 toId = toNode->GetId();

	// 真正的Dijkstra派发到线程池异步算——每天9点/12点这类所有有工作的市民同一时刻集中
	// 触发调度，单次调用在这张车道级导航图上也不便宜，堆在游戏线程上会造成持续的卡顿
	// (PIE验证反馈：从9点持续卡到9点半，即使Populace::Tick每帧已经只处理一个job
	// timer)。map->FindPedestrianPath是纯读操作(见map.h/map.md)，地图生成完成后
	// pedestrianNavGraph/navAnchorNodes不会再被修改，多个后台线程并发只读安全。
	pendingPathfindingTasks.fetch_add(1, std::memory_order_relaxed);
	TWeakObjectPtr<UForeverPopulaceFrameworkComponent> weakThis(this);
	Async(EAsyncExecution::ThreadPool, [weakThis, mapPtr, fromId, toId, citizen, destination, bInTransit, transitFeet, serial]() {
		// 正在关闭：队列里还没轮到的任务直接跳过，不然排着很多Dijkstra时退出会卡很久。
		if (bCancelPathfinding.load(std::memory_order_acquire)) {
			pendingPathfindingTasks.fetch_sub(1, std::memory_order_release);
			return;
		}
		TArray<FVector> waypoints;
		if (bInTransit) {
			// 在路上：起点是当前位置，接到离它最近的行人图节点（PlanPedestrianRoute内部逐个试最近的几个），
			// 路径第一个点就是当前位置，这样Core推算/Actor都从原地接着走，不会跳回节点。
			Map::VehicleRoutePlan plan = mapPtr->PlanPedestrianRoute(nullptr,
				static_cast<float>(transitFeet.X / POPULACE_WORLD_SCALE), static_cast<float>(transitFeet.Y / POPULACE_WORLD_SCALE), destination);
			if (plan.status == Map::VehicleRoutePlan::OK && !plan.points.empty()) {
				waypoints.Add(transitFeet);
				for (const Map::NavPoint& point : plan.points) {
					waypoints.Add(FVector(point.x, point.y, point.z) * POPULACE_WORLD_SCALE);
				}
			}
		}
		else {
			std::vector<const Node*> path = mapPtr->FindPedestrianPath(fromId, toId);
			waypoints.Reserve(static_cast<int32>(path.size()));
			for (const Node* node : path) {
				waypoints.Add(FVector(node->GetX(), node->GetY(), node->GetZ()) * POPULACE_WORLD_SCALE);
			}
		}
		// weakThis.Get()在后台线程上解析是UE支持的用法(内部靠序列号判定，不需要跑在
		// 游戏线程上)——只用来判断这个组件是不是已经被销毁，判断结果之后立刻只做入队
		// 这一个动作，不做任何其它UObject操作。就算组件已经销毁、这里什么也不做，也不会
		// 影响下面的计数器递减(计数器是static的，不依赖这个组件实例，见.h的注释)。
		if (UForeverPopulaceFrameworkComponent* component = weakThis.Get()) {
			component->pathResultQueue.Enqueue({ citizen, destination, MoveTemp(waypoints), serial });
		}
		pendingPathfindingTasks.fetch_sub(1, std::memory_order_release);
		});
}

void UForeverPopulaceFrameworkComponent::ApplyWalkResult(Citizen* citizen, Room* destination,
	const TArray<FVector>& waypoints) {
	if (!citizen || !destination || citizen->IsDead()) return;

	// 重新查一次activeInstances——从RequestWalk发起(可能异步经过若干帧)到这里落地之间，
	// citizen完全可能被流式销毁/重新生成过，也可能刚被玩家占有，不能沿用发起那一刻的
	// existing指针/占有状态。
	TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen);
	if (existing && *existing && (*existing)->IsPlayerControlled()) {
		return;
	}

	if (waypoints.Num() > 0 && existing && *existing) {
		// WalkTo会取代正在走的旧路线；出发后人在路上，不属于任何房间，走到了NotifyArrived才换成目的地。
		(*existing)->WalkTo(waypoints, destination);
		citizen->SetCurrentRoom(nullptr);
		return;
	}

	if (existing && *existing) {
		// 有已生成的Actor，但寻路失败(起点/终点没有导航节点，或图不连通)：不能只改
		// Core状态、放着这个可见的Actor不动——那样Citizen逻辑上已经"到家"了，但玩家
		// 眼前的人会一直冻结在原地。直接把Actor也瞬移过去。(用户曾报告过citizen冻结
		// 不动的现象，排查后确认那次的真正原因是CharacterMovementComponent缺
		// bRunPhysicsWithNoController、和这里的寻路失败无关，见CitizenElement.md
		// "已修复的bug"一节——这个分支仍然保留，处理寻路真的失败这一更罕见的情况。)
		Room* previous = citizen->GetCurrentRoom();
		UE_LOG(LogTemp, Warning, TEXT("ApplyWalkResult: no path for visible citizen %s (%s -> %s), teleported."),
			UTF8_TO_TCHAR(citizen->GetName().c_str()),
			UTF8_TO_TCHAR(previous ? previous->GetAddress().c_str() : "?"),
			UTF8_TO_TCHAR(destination->GetAddress().c_str()));
		(*existing)->TeleportToRoom(destination);
		ReportArrival(citizen, previous);
		return;
	}

	// 这个citizen当前没有生成的ACitizenElement(不在流式加载范围内)：路径非空时交给Core按
	// 时间推算沿途位置(流式扫描会在位置进入生成距离时把它生成出来接着走，走完才换房间)；
	// 路径为空(寻路失败)才直接瞬移逻辑状态。
	if (waypoints.Num() > 1) {
		const ACitizenElement* defaults = GetDefault<ACitizenElement>();
		float speedUU = defaults->GetCharacterMovement()->MaxWalkSpeed;
		std::vector<Citizen::WalkPoint> path;
		path.reserve(waypoints.Num());
		for (const FVector& point : waypoints) {
			path.push_back({ static_cast<float>(point.X / POPULACE_WORLD_SCALE),
				static_cast<float>(point.Y / POPULACE_WORLD_SCALE), static_cast<float>(point.Z / POPULACE_WORLD_SCALE) });
		}
		citizen->BeginWalk(path, destination, GetWorld()->GetTimeSeconds(), speedUU * kUnspawnedWalkSpeedScale / POPULACE_WORLD_SCALE);
		citizen->SetCurrentRoom(nullptr); // 在路上，见上
		return;
	}
	Room* previous = citizen->GetCurrentRoom();
	citizen->EndWalk();
	citizen->SetCurrentRoom(destination);
	citizen->ClearPosition();
	ReportArrival(citizen, previous);
}

void UForeverPopulaceFrameworkComponent::TeleportCitizen(Citizen* citizen, Room* destination) {
	if (!citizen || !destination) return;
	if (citizen->IsDead()) {
		UE_LOG(LogTemp, Warning, TEXT("npc_teleport: %s is dead, skipped (use npc_revive first)."),
			UTF8_TO_TCHAR(citizen->GetName().c_str()));
		return;
	}

	TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen);
	if (existing && *existing) {
		if ((*existing)->IsPlayerControlled()) {
			UE_LOG(LogTemp, Warning, TEXT("npc_teleport: %s is controlled by the player, skipped."),
				UTF8_TO_TCHAR(citizen->GetName().c_str()));
			return;
		}
		++latestWalkRequest.FindOrAdd(citizen); // 在途的寻路结果作废
		Room* previous = citizen->GetCurrentRoom();
		(*existing)->TeleportToRoom(destination);
		ReportArrival(citizen, previous);
		return;
	}

	++latestWalkRequest.FindOrAdd(citizen); // 在途的寻路结果作废
	Room* previous = citizen->GetCurrentRoom();
	citizen->EndWalk();
	citizen->SetCurrentRoom(destination);
	citizen->ClearPosition();
	ReportArrival(citizen, previous);
}

void UForeverPopulaceFrameworkComponent::DrainPathResults() {
	FPendingWalkResult result;
	while (pathResultQueue.Dequeue(result)) {
		const uint32* latest = latestWalkRequest.Find(result.citizen);
		if (!latest || *latest != result.serial) continue; // 已被更新的导航/瞬移取代
		ApplyWalkResult(result.citizen, result.destination, result.waypoints);
	}
}

bool UForeverPopulaceFrameworkComponent::GetTransitPosition(Citizen* citizen, FVector& outFeet) const {
	if (!citizen) return false;
	if (const TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen); existing && *existing) {
		const ACitizenController* controller = (*existing)->GetCitizenController();
		if (!controller || !controller->HasCommands()) return false;
		// Actor位置是胶囊体中心，路径点是脚底高度。
		outFeet = (*existing)->GetActorLocation() - FVector(0.f, 0.f, (*existing)->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		return true;
	}
	if (!citizen->IsWalking() || !GetWorld()) return false;
	std::vector<Citizen::WalkPoint> remaining;
	citizen->GetRemainingWalk(GetWorld()->GetTimeSeconds(), remaining);
	if (remaining.empty()) return false;
	outFeet = FVector(remaining[0].x, remaining[0].y, remaining[0].z) * POPULACE_WORLD_SCALE;
	return true;
}

void UForeverPopulaceFrameworkComponent::WaitForPendingPathfinding() {
	bCancelPathfinding.store(true, std::memory_order_release);
	while (pendingPathfindingTasks.load(std::memory_order_acquire) > 0) {
		FPlatformProcess::Sleep(0.001f);
	}
}

bool UForeverPopulaceFrameworkComponent::PlanWalkPath(Citizen* citizen, Room* destination, const FVector& from,
	TArray<FVector>& outWaypoints) const {
	if (!map || !citizen || !destination) return false;
	Map::VehicleRoutePlan plan = map->PlanPedestrianRoute(citizen->GetCurrentRoom(),
		from.X / POPULACE_WORLD_SCALE, from.Y / POPULACE_WORLD_SCALE, destination);
	if (plan.status != Map::VehicleRoutePlan::OK) return false;
	outWaypoints.Reset();
	for (const Map::NavPoint& p : plan.points) {
		outWaypoints.Add(FVector(p.x, p.y, p.z) * POPULACE_WORLD_SCALE);
	}
	return outWaypoints.Num() > 0;
}

void UForeverPopulaceFrameworkComponent::NotifyArrived(Citizen* citizen, Room* destination) {
	if (!citizen || !destination || citizen->IsDead()) return;
	Room* previous = citizen->GetCurrentRoom();
	citizen->SetCurrentRoom(destination);
	ReportArrival(citizen, previous);
}

void UForeverPopulaceFrameworkComponent::ReportArrival(Citizen* citizen, Room* previousRoom) {
	if (!citizen || citizen->IsDead() || citizen->GetCurrentRoom() == previousRoom) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (UForeverStoryFrameworkComponent* story = framework ? framework->GetStoryFramework() : nullptr) {
		story->BroadcastNpcArrive(citizen, citizen->GetCurrentRoom());
	}
}

void UForeverPopulaceFrameworkComponent::StartTestNavigation(const FString& citizenName) {
	ACitizenElement* element = FindOrSpawnCitizenByName(citizenName);
	if (!element) return;
	ACitizenController* controller = element->GetCitizenController();
	TArray<FVector> path;
	Room* destination = nullptr;
	if (!controller || !controller->GetRemainingWaypoints(path, destination) || path.Num() < 2) return;
	if (UForeverTrafficFrameworkComponent* trafficLine = GetOwner() ? GetOwner()->FindComponentByClass<UForeverTrafficFrameworkComponent>() : nullptr) {
		TWeakObjectPtr<ACitizenController> weakController(controller);
		trafficLine->TrackTestNavigationLine([weakController](TArray<FVector>& outPath) {
			const ACitizenController* c = weakController.Get();
			Room* ignored = nullptr;
			return c && c->GetRemainingWaypoints(outPath, ignored);
		});
	}
}

void UForeverPopulaceFrameworkComponent::HandleElementEndPlay(Citizen* citizen, ACitizenElement* element) {
	if (!citizen) return;
	TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen);
	if (!existing || existing->Get() != element) return;
	activeInstances.Remove(citizen);
}

void UForeverPopulaceFrameworkComponent::HandleCitizenDeath(Citizen* citizen) {
	if (!citizen) return;

	if (TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen)) {
		if (*existing) {
			(*existing)->Destroy();
		}
		activeInstances.Remove(citizen);
	}
}

ACitizenElement* UForeverPopulaceFrameworkComponent::FindOrSpawnCitizenByName(const FString& name) {
	for (Citizen* citizen : allCitizens) {
		if (!citizen || citizen->IsDead()) continue;
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

	// 世界还在分阶段生成：game_start广播(给市民加交互选项)之前不能流式生成市民，否则生成时烘焙的
	// 选项快照是空的，见AForeverFrameworkActor::BeginPlay。
	if (const AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(GetOwner())) {
		if (!frameworkActor->IsInitialGenerationDone()) return;
	}

	// 把RequestWalk异步派发到线程池、已经算完的寻路结果落地——这一步必须在游戏线程上做
	// (ApplyWalkResult会碰activeInstances/ACitizenElement这些UObject相关状态)，
	// pathResultQueue本身的Dequeue是线程安全的，但落地逻辑不能搬到后台线程去做。
	DrainPathResults();

	// 屏幕左上角实时显示寻路队列长度（已派发到线程池、还没算完的任务数，含正在跑的）。
	if (bShowPathfindingQueue && GEngine) {
		GEngine->AddOnScreenDebugMessage(9102, 0.f, FColor::Cyan,
			FString::Printf(TEXT("寻路队列: %d"), pendingPathfindingTasks.load(std::memory_order_relaxed)));
	}

	int32 total = allCitizens.Num();
	if (!map || total == 0) return;

	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn) return;
	FVector playerLoc = pawn->GetActorLocation();

	float spawnDistUE = citizenSpawnDistance * POPULACE_WORLD_SCALE;
	float despawnDistUE = citizenDespawnDistance * POPULACE_WORLD_SCALE;
		int32 batchCount = FMath::Clamp(total / FMath::Max(1, streamingBatchStride), 1, total);

	int32 spawnCount = 0;
	const double now = GetWorld()->GetTimeSeconds();

	// 静止市民和移动中的市民分开管：静止的用原来的生成/销毁距离、最多maxStationaryCitizens个；移动中的
	// (Actor有正在执行的走路命令，或Core正在推算它的路线)距离翻倍、最多maxMovingCitizens个。
	// 免销毁的(被玩家操控等)不计数。
	auto isMoving = [](const ACitizenElement* element) {
		const ACitizenController* controller = element->GetCitizenController();
		return controller && controller->HasCommands();
	};
	int32 stationaryCount = 0, movingCount = 0;
	for (const auto& [activeCitizen, activeElement] : activeInstances) {
		if (!activeElement || activeElement->IsDespawnExempt()) continue;
		(isMoving(activeElement) ? movingCount : stationaryCount)++;
	}
	
	int32 processed = 0;
	for (int32 i = 0; i < batchCount; i++) {
		// 本帧生成数已到上限：停止扫描，下一帧从还没处理的位置接着来。
		if (spawnCount >= FMath::Max(1, maxSpawnsPerFrame)) break;
		processed = i + 1;
		int32 idx = (pollCursor + i) % total;
		Citizen* citizen = allCitizens[idx];
		if (!citizen || citizen->IsDead()) continue;

		TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen);

		if (!existing) {
			// 没有Actor但正在路上的市民：Core按时间推算位置(走完则已换成目的地房间)，
			// 之后照常按位置判断要不要生成。
			if (citizen->IsWalking()) {
				Room* previous = citizen->GetCurrentRoom();
				if (citizen->AdvanceWalk(now)) ReportArrival(citizen, previous);
			}
			float worldX, worldY, worldZ;
			if (!ComputeLogicalPosition(citizen, worldX, worldY, worldZ)) continue;
			const bool moving = citizen->IsWalking();
			if (moving ? movingCount >= maxMovingCitizens : stationaryCount >= maxStationaryCitizens) continue;
			const float spawnLimit = moving ? spawnDistUE * movingDistanceScale : spawnDistUE;
			if (StreamingDistance(playerLoc, FVector(worldX, worldY, worldZ)) < spawnLimit) {
				ACitizenElement* element = SpawnCitizen(citizen);
				spawnCount++;
				if (element) (moving ? movingCount : stationaryCount)++;
				if (element && citizen->IsWalking()) {
					// 路径点是脚底高度，Actor位置是胶囊体中心，抬一个半高再接着走剩余路段。
					std::vector<Citizen::WalkPoint> remaining;
					citizen->GetRemainingWalk(now, remaining);
					TArray<FVector> waypoints;
					for (const Citizen::WalkPoint& point : remaining) {
						waypoints.Add(FVector(point.x, point.y, point.z) * POPULACE_WORLD_SCALE);
					}
					Room* destination = citizen->GetWalkDestination();
					citizen->EndWalk();
					element->AddActorWorldOffset(FVector(0.f, 0.f, element->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
					element->WalkTo(waypoints, destination);
				}
			}
		}
		else {
			ACitizenElement* element = *existing;
			if (!element) {
				activeInstances.Remove(citizen);
				continue;
			}
			const bool moving = isMoving(element);
			const float despawnLimit = moving ? despawnDistUE * movingDistanceScale : despawnDistUE;
			// 只按距离销毁；某一类数量超过上限(比如走路的人到站变成静止的而静止名额已满)就超着，只是不再生成新的。
			if (!element->IsDespawnExempt() && StreamingDistance(playerLoc, element->GetActorLocation()) > despawnLimit) {
				// 走远到销毁距离之外：先把当前位置转回地图单位写回Citizen(这样下次重新进入
				// 范围直接用这份记录，不再随机抖动一次)，再销毁Actor。
				FVector loc = element->GetActorLocation();
				citizen->SetPosition(loc.X / POPULACE_WORLD_SCALE, loc.Y / POPULACE_WORLD_SCALE,
					loc.Z / POPULACE_WORLD_SCALE);
				// 还在走路：剩余路径交还Core继续按时间推算，不然目的地就丢了(currentRoom还是出发地)。
				TArray<FVector> remainingPath;
				Room* walkDestination = nullptr;
				ACitizenController* controller = element->GetCitizenController();
				if (controller && controller->GetRemainingWaypoints(remainingPath, walkDestination) && remainingPath.Num() > 1) {
					std::vector<Citizen::WalkPoint> path;
					for (const FVector& point : remainingPath) {
						path.push_back({ static_cast<float>(point.X / POPULACE_WORLD_SCALE),
							static_cast<float>(point.Y / POPULACE_WORLD_SCALE), static_cast<float>(point.Z / POPULACE_WORLD_SCALE) });
					}
					float speedUU = element->GetCharacterMovement()->MaxWalkSpeed;
					citizen->BeginWalk(path, walkDestination, now, speedUU * kUnspawnedWalkSpeedScale / POPULACE_WORLD_SCALE);
				}
				element->Destroy();
				activeInstances.Remove(citizen);
			}
		}
	}

	pollCursor = (pollCursor + processed) % total;
}

TArray<UForeverPopulaceFrameworkComponent::FWalkingSnapshot> UForeverPopulaceFrameworkComponent::SyncPositionsToCore() {
	// 先把已经算完的寻路结果落地(和TickComponent里同一套)，这些市民也开始走路了。
	DrainPathResults();

	TArray<FWalkingSnapshot> walking;
	// Core里推算中的市民(没有Actor)：存档里按"已经到了"记，存完RestoreAfterSave还原。
	for (Citizen* citizen : allCitizens) {
		if (!citizen || !citizen->IsWalking()) continue;
		FWalkingSnapshot snapshot;
		snapshot.citizen = citizen;
		snapshot.currentRoom = citizen->GetCurrentRoom();
		snapshot.hasPosition = citizen->HasPosition();
		citizen->GetPosition(snapshot.x, snapshot.y, snapshot.z);
		walking.Add(snapshot);
		citizen->SetCurrentRoom(citizen->GetWalkDestination());
		citizen->ClearPosition();
	}
	for (auto& [citizen, element] : activeInstances) {
		if (!citizen || !element) continue;
		FVector loc = element->GetActorLocation();
		citizen->SetPosition(loc.X / POPULACE_WORLD_SCALE, loc.Y / POPULACE_WORLD_SCALE, loc.Z / POPULACE_WORLD_SCALE);

		Room* destination = element->GetWalkDestination();
		if (!destination) continue;
		FWalkingSnapshot snapshot;
		snapshot.citizen = citizen;
		snapshot.currentRoom = citizen->GetCurrentRoom();
		snapshot.hasPosition = citizen->HasPosition();
		citizen->GetPosition(snapshot.x, snapshot.y, snapshot.z);
		walking.Add(snapshot);

		citizen->SetCurrentRoom(destination);
		citizen->ClearPosition();
	}
	return walking;
}

void UForeverPopulaceFrameworkComponent::RestoreAfterSave(const TArray<FWalkingSnapshot>& walking) {
	for (const FWalkingSnapshot& snapshot : walking) {
		if (!snapshot.citizen) continue;
		snapshot.citizen->SetCurrentRoom(snapshot.currentRoom);
		if (snapshot.hasPosition) snapshot.citizen->SetPosition(snapshot.x, snapshot.y, snapshot.z);
		else snapshot.citizen->ClearPosition();
	}
}

void UForeverPopulaceFrameworkComponent::ForEachActiveCitizen(TFunctionRef<void(ACitizenElement*)> visitor) const {
	for (const auto& [citizen, actor] : activeInstances) {
		if (IsValid(actor)) visitor(actor);
	}
}

ACitizenElement* UForeverPopulaceFrameworkComponent::FindActiveCitizen(Citizen* citizen) const {
	const TObjectPtr<ACitizenElement>* found = activeInstances.Find(citizen);
	return found ? found->Get() : nullptr;
}
