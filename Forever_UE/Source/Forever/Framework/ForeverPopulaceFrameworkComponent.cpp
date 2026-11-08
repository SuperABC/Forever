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

// 和CitizenElement.cpp的CITIZEN_WORLD_SCALE同一个值，各文件各自维护一份，不抽公共头(约定见BuildingElement.cpp顶部注释)。
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

	// 和CitizenElement.cpp里同名函数同一个公式，只用来估算"还没有Actor的citizen"的逻辑位置，
	// 不加随机抖动(抖动是ACitizenElement::Init()生成时的一次性效果)。返回false表示既没有记录位置
	// 也没有房间，无法估算。
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

		Room* room = citizen->GetCurrentRoom(); // 物理位置，不是家(GetRoom())，见citizen.h/room.h的三概念说明
		// building必须从room->GetParentBuilding()反查，不能用citizen->GetBuilding()：
		// 后者是"家"所在的building，currentRoom经RequestWalk换到工作单位的room后两者不再同属一栋楼，
		// 混用会用错building变换算出错误的世界坐标(典型症状：上班时间商店里看不到店员)。
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
	pendingRestoreSpawns.Reset();
	const double now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	for (Citizen* citizen : inPopulace->GetCitizens()) {
		if (!citizen) continue;
		allCitizens.Add(citizen);
		// 读档恢复：Core行进的起算时间对齐到新关卡的游戏时钟；存档时已实例化的市民排队强制生成。
		citizen->RestartWalk(now);
		if (citizen->WasSpawned() && citizen->IsActive()) pendingRestoreSpawns.Add(citizen);
		citizen->SetWasSpawned(false);
	}
}

void UForeverPopulaceFrameworkComponent::HandOffWalkToElement(Citizen* citizen, ACitizenElement* element, double now) {
	if (!citizen || !element || !citizen->IsWalking()) return;
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
		spawnTimes.Add(citizen, GetWorld()->GetTimeSeconds());
			}
	return element;
}

void UForeverPopulaceFrameworkComponent::RequestWalk(Citizen* citizen, Room* destination) {
	if (!citizen || !destination || !map) return;
	if (!citizen->IsActive()) return; // 死人和离城的人不再被调度，见Citizen::Die/Depart

	TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen);
	if (existing && *existing && (*existing)->IsPlayerControlled()) {
		// 正被玩家占有(市民自带的ACitizenController不算，只看玩家Controller)：
		// Job调度这次先整个不生效，被占有的市民不会自己上下班——不管是走路动画还是瞬移
		// 都不做，等玩家取消占有之后下一次调度再正常处理。
		return;
	}

	// 跳时间期间路走不完(寻路异步、走路要帧)，同步瞬移到目的地并触发npc_arrive。
	if (bTimeSkipping) {
		TeleportCitizen(citizen, destination);
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

	// Dijkstra派发到线程池异步算(整点集中调度时放游戏线程会持续卡顿)。FindPedestrianPath是纯读操作，
	// 地图生成完成后导航图不再被写，多线程并发只读安全。
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
		// weakThis.Get()可在后台线程解析，只用来判断组件是否已销毁，之后只做入队这一个动作。
		// 计数器是static的，组件已销毁也照常递减。
		if (UForeverPopulaceFrameworkComponent* component = weakThis.Get()) {
			component->pathResultQueue.Enqueue({ citizen, destination, MoveTemp(waypoints), serial });
		}
		pendingPathfindingTasks.fetch_sub(1, std::memory_order_release);
		});
}

void UForeverPopulaceFrameworkComponent::ApplyWalkResult(Citizen* citizen, Room* destination,
	const TArray<FVector>& waypoints) {
	if (!citizen || !destination || !citizen->IsActive()) return;

	// 重新查一次activeInstances：从发起到落地隔了若干帧，citizen可能已被流式销毁/重新生成或被玩家占有。
	TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen);
	if (existing && *existing && (*existing)->IsPlayerControlled()) {
		return;
	}

	// 新的一次行走开始：按行走实例化概率决定这次走路是否参与流式实例化(见Citizen::RollWalkInstantiable)。
	citizen->RollWalkInstantiable(walkInstantiateProbability);

	if (waypoints.Num() > 0 && existing && *existing) {
		// WalkTo会取代正在走的旧路线；出发后人在路上，不属于任何房间，走到了NotifyArrived才换成目的地。
		(*existing)->WalkTo(waypoints, destination);
		citizen->SetCurrentRoom(nullptr);
		return;
	}

	if (existing && *existing) {
		// 有已生成的Actor但寻路失败(起点/终点没有导航节点，或图不连通)：不能只改Core状态，
		// 否则逻辑上已"到家"而可见的Actor冻结在原地，所以Actor也瞬移过去。
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
	if (!citizen->IsActive()) {
		UE_LOG(LogTemp, Warning, TEXT("npc_teleport: %s is dead or departed, skipped (use npc_revive first)."),
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

void UForeverPopulaceFrameworkComponent::BeginTimeSkip() {
	bTimeSkipping = true;

	// 已算完还没落地的寻路结果丢掉；还在线程池里的任务靠序号+1作废(晚到的结果DrainPathResults会丢弃)。
	FPendingWalkResult discarded;
	while (pathResultQueue.Dequeue(discarded)) {}
	for (TPair<Citizen*, uint32>& pair : latestWalkRequest) ++pair.Value;

	// 在途的行走一律在起点时刻到达目的地。
	for (int32 i = 0; i < allCitizens.Num(); i++) {
		Citizen* citizen = allCitizens[i];
		if (!citizen || !citizen->IsActive()) continue;

		Room* destination = nullptr;
		if (TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen); existing && *existing) {
			if ((*existing)->IsPlayerControlled()) continue;
			ACitizenController* controller = (*existing)->GetCitizenController();
			TArray<FVector> remaining;
			if (controller) controller->GetRemainingWaypoints(remaining, destination);
		}
		else if (citizen->IsWalking()) {
			destination = citizen->GetWalkDestination();
		}
		if (destination) TeleportCitizen(citizen, destination);
	}
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
	if (!citizen || !destination || !citizen->IsActive()) return;
	Room* previous = citizen->GetCurrentRoom();
	citizen->SetCurrentRoom(destination);
	ReportArrival(citizen, previous);
}

void UForeverPopulaceFrameworkComponent::ReportArrival(Citizen* citizen, Room* previousRoom) {
	if (!citizen || !citizen->IsActive() || citizen->GetCurrentRoom() == previousRoom) return;
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
	spawnTimes.Remove(citizen);
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
		if (!citizen || !citizen->IsActive()) continue;
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

	// 已算完的寻路结果落地：必须在游戏线程(ApplyWalkResult会碰activeInstances/ACitizenElement)。
	DrainPathResults();

	// 屏幕左上角实时显示寻路队列长度（已派发到线程池、还没算完的任务数，含正在跑的）。
	if (bShowPathfindingQueue && GEngine) {
		GEngine->AddOnScreenDebugMessage(9102, 0.f, FColor::Cyan,
			FString::Printf(TEXT("寻路队列: %d"), pendingPathfindingTasks.load(std::memory_order_relaxed)));
	}

	int32 total = allCitizens.Num();
	if (!map || total == 0) return;

	// 读档后先把存档时已实例化的市民全部生成出来(不看距离和数量上限)，再恢复正常的流式扫描：
	// 否则按距离重新选的16个可能和存档前不是同一批。
	if (pendingRestoreSpawns.Num() > 0) {
		const double restoreNow = GetWorld()->GetTimeSeconds();
		int32 restored = 0;
		while (pendingRestoreSpawns.Num() > 0 && restored < FMath::Max(1, maxSpawnsPerFrame)) {
			Citizen* citizen = pendingRestoreSpawns.Pop(EAllowShrinking::No);
			if (!citizen || !citizen->IsActive() || activeInstances.Contains(citizen)) continue;
			if (ACitizenElement* element = SpawnCitizen(citizen)) {
				HandOffWalkToElement(citizen, element, restoreNow);
				restored++;
			}
		}
		return;
	}

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
		if (!citizen || !citizen->IsActive()) continue;

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
			const float spawnLimit = moving ? spawnDistUE * movingDistanceScale : spawnDistUE;
			const bool inRange = StreamingDistance(playerLoc, FVector(worldX, worldY, worldZ)) < spawnLimit;
			// 统计进入生成距离的行走者(不论这趟是否可实例化)，用来动态调行走实例化概率。
			if (moving && inRange) sweepWalkersInRange++;
			// 这次行走被概率排除在实例化之外：一直由Core推算，走完为止。
			if (moving && !citizen->IsWalkInstantiable()) continue;
			if (moving ? movingCount >= maxMovingCitizens : stationaryCount >= maxStationaryCitizens) continue;
			if (inRange) {
				ACitizenElement* element = SpawnCitizen(citizen);
				spawnCount++;
				if (element) (moving ? movingCount : stationaryCount)++;
				if (element) HandOffWalkToElement(citizen, element, now);
			}
		}
		else {
			ACitizenElement* element = *existing;
			if (!element) {
				activeInstances.Remove(citizen);
				continue;
			}
			const bool moving = isMoving(element);
			if (moving) sweepWalkersInRange++;
			const float despawnLimit = moving ? despawnDistUE * movingDistanceScale : despawnDistUE;
			// 只按距离销毁；某一类数量超过上限(比如走路的人到站变成静止的而静止名额已满)就超着，只是不再生成新的。
			if (!element->IsDespawnExempt() && StreamingDistance(playerLoc, element->GetActorLocation()) > despawnLimit &&
				now - spawnTimes.FindRef(citizen) >= minCitizenLifetime) {
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

	// 扫完一整轮：本轮范围内行走者数x刷新峰值，概率取1.5*y/x(y=maxMovingCitizens)，只减不增。
	// 峰值(而不是当前值)保证高峰过后概率不会回弹，所以第二次高峰起才真正生效。
	if (pollCursor + processed >= total) {
		peakWalkersInRange = FMath::Max(peakWalkersInRange, sweepWalkersInRange);
		sweepWalkersInRange = 0;
		if (peakWalkersInRange > 0) {
			walkInstantiateProbability = FMath::Min(walkInstantiateProbability,
				1.5f * FMath::Max(1, maxMovingCitizens) / peakWalkersInRange);
		}
	}
	pollCursor = (pollCursor + processed) % total;
}

TArray<UForeverPopulaceFrameworkComponent::FWalkingSnapshot> UForeverPopulaceFrameworkComponent::SyncPositionsToCore() {
	// 先把已经算完的寻路结果落地(和TickComponent里同一套)，这些市民也开始走路了。
	DrainPathResults();

	const double now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	TArray<FWalkingSnapshot> walking;
	for (Citizen* citizen : allCitizens) {
		if (!citizen) continue;
		const TObjectPtr<ACitizenElement>* active = activeInstances.Find(citizen);
		citizen->SetWasSpawned(active && *active);
		// Core里推算中的市民(没有Actor)：路径改写成从当前位置起的剩余路径，存档里是即时状态，不用还原。
		if (citizen->IsWalking()) citizen->RebaseWalk(now);
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

		// Actor正在走：把剩余路径(首点=当前位置，脚底高度)临时交给Core行进模拟，存档里记即时状态，
		// 速度取Actor的真实速度；存完RestoreAfterSave撤掉。
		TArray<FVector> remainingPath;
		Room* walkDestination = nullptr;
		const ACitizenController* controller = element->GetCitizenController();
		if (controller && controller->GetRemainingWaypoints(remainingPath, walkDestination) && remainingPath.Num() > 0) {
			std::vector<Citizen::WalkPoint> path;
			for (const FVector& point : remainingPath) {
				path.push_back({ static_cast<float>(point.X / POPULACE_WORLD_SCALE),
					static_cast<float>(point.Y / POPULACE_WORLD_SCALE), static_cast<float>(point.Z / POPULACE_WORLD_SCALE) });
			}
			float speedUU = element->GetCharacterMovement()->MaxWalkSpeed;
			citizen->BeginWalk(path, walkDestination, now, speedUU / POPULACE_WORLD_SCALE);
			snapshot.bCoreWalk = citizen->IsWalking();
			if (snapshot.bCoreWalk) {
				walking.Add(snapshot);
				continue;
			}
		}

		// 路线还没开始(指令还没展开成路径)等没法取剩余路径的情况：退回按"已经走到目的地"记。
		walking.Add(snapshot);
		citizen->SetCurrentRoom(destination);
		citizen->ClearPosition();
	}
	return walking;
}

void UForeverPopulaceFrameworkComponent::RestoreAfterSave(const TArray<FWalkingSnapshot>& walking) {
	for (const FWalkingSnapshot& snapshot : walking) {
		if (!snapshot.citizen) continue;
		if (snapshot.bCoreWalk) snapshot.citizen->EndWalk();
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
