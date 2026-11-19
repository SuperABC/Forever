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
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HAL/ThreadingBase.h"
#include "Templates/UniquePtr.h"

#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "map/general_geometry.h"
#include "populace/populace.h"
#include "populace/citizen.h"
#include "traffic/traffic.h"
#include "traffic/vehicle.h"
#include "player/player.h"
#include "society/society.h"
#include "society/organization.h"
#include "society/job.h"
#include "story/script.h"

std::atomic<int32> UForeverPopulaceFrameworkComponent::pendingPathfindingTasks{ 0 };
std::atomic<bool> UForeverPopulaceFrameworkComponent::bCancelPathfinding{ false };

// 和CitizenElement.cpp的CITIZEN_WORLD_SCALE同一个值，各文件各自维护一份，不抽公共头(约定见BuildingElement.cpp顶部注释)。
#define POPULACE_WORLD_SCALE 1000.f
#define POPULACE_HEIGHT_EPSILON 10.f

// 流式生成/销毁判断距离时Z方向的权重：不同楼层的人一般互相看不到，Z差放大5倍再算距离，
// 头顶/脚下楼层的市民就不会因为三维距离近而被生成。生成和销毁两个阈值用同一个度量，迟滞区间才一致。
#define POPULACE_VERTICAL_DISTANCE_WEIGHT 5.f

namespace {
	// 寻路专用线程池：整点几千个Dijkstra同时派发，放进引擎共用的GThreadPool会把引擎自己用线程池的工作
	// (异步加载、纹理/网格构建等)排在后面，第一个市民出现时同步等这些就卡几秒。专用池只占一半物理核、低优先级。
	TUniquePtr<FQueuedThreadPool> pathfindingPool;

	FQueuedThreadPool& GetPathfindingPool() {
		if (!pathfindingPool) {
			pathfindingPool.Reset(FQueuedThreadPool::Allocate());
			const int32 threads = FMath::Max(1, FPlatformMisc::NumberOfCores() / 2);
			pathfindingPool->Create(threads, 256 * 1024, TPri_BelowNormal, TEXT("ForeverPathfindingPool"));
		}
		return *pathfindingPool;
	}

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

float UForeverPopulaceFrameworkComponent::StreamingDistanceBetween(const FVector& a, const FVector& b) {
	return StreamingDistance(a, b);
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

	const double spawnStart = FPlatformTime::Seconds();
	ACitizenElement* element = GetWorld()->SpawnActor<ACitizenElement>();
	const double spawnedSeconds = FPlatformTime::Seconds() - spawnStart;
	if (element) {
		element->Init(citizen, this);
		// 卡顿排查：生成一个市民超过30ms就打日志，区分SpawnActor和Init(头像等)。
		const double totalSeconds = FPlatformTime::Seconds() - spawnStart;
		if (totalSeconds > 0.03) {
			UE_LOG(LogTemp, Warning, TEXT("SpawnCitizen: %s took %.1f ms (SpawnActor %.1f ms, Init %.1f ms)."),
				UTF8_TO_TCHAR(citizen->GetName().c_str()), totalSeconds * 1000.0, spawnedSeconds * 1000.0, (totalSeconds - spawnedSeconds) * 1000.0);
		}
		activeInstances.Add(citizen, element);
		spawnTimes.Add(citizen, GetWorld()->GetTimeSeconds());
			}
	return element;
}

Traffic* UForeverPopulaceFrameworkComponent::GetTrafficCore() const {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	return framework ? framework->GetTraffic() : nullptr;
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

	// 开车通勤：自己的车停在当前所在的建筑、目的地建筑有空闲车位才开车，否则照常走路。
	{
		if (Traffic* traffic = GetTrafficCore()) {
			if (Traffic::TaskTrip* task = traffic->FindTaskByWorker(citizen)) {
					if (task->phase == Traffic::TaskPhase::Driving) {
						UE_LOG(LogTemp, Log, TEXT("RequestWalk: %s is driving a task vehicle, request ignored."), UTF8_TO_TCHAR(citizen->GetName().c_str()));
						return;
					}
					AbortTaskOfWorker(citizen); // 还在走路的任务被新请求取代
				}
				if (Traffic::CommuteTrip* trip = traffic->FindTrip(citizen)) {
					if (trip->phase == Traffic::CommutePhase::Driving) {
					UE_LOG(LogTemp, Log, TEXT("RequestWalk: %s is riding in a car, request ignored."), UTF8_TO_TCHAR(citizen->GetName().c_str()));
					return; // 人在车里，等车到了再说
				}
				CancelCommute(citizen); // 还在走向车：被新请求取代
			}
			if (traffic->BeginCommute(citizen, destination)) {
				StartCommute(citizen);
				return;
			}
		}
	}
	RequestWalkInternal(citizen, destination);
}

void UForeverPopulaceFrameworkComponent::StartCommute(Citizen* citizen) {
	Traffic* traffic = GetTrafficCore();
	Traffic::CommuteTrip* trip = traffic ? traffic->FindTrip(citizen) : nullptr;
	if (!trip) return;
	if (citizen->GetCurrentRoom() == trip->parkOrigin) {
		BoardCar(citizen); // 已经在车所在的停车房里
		return;
	}
	RequestWalkInternal(citizen, trip->parkOrigin);
}

void UForeverPopulaceFrameworkComponent::BoardCar(Citizen* citizen) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Traffic* traffic = GetTrafficCore();
	UForeverTrafficFrameworkComponent* trafficFramework = framework ? framework->GetTrafficFramework() : nullptr;
	Traffic::CommuteTrip* trip = traffic ? traffic->FindTrip(citizen) : nullptr;
	if (!trip || !trafficFramework || !trip->vehicle) {
		CancelCommute(citizen);
		return;
	}
	// 市民消失：销毁Actor，Core里的Citizen保留，处在"在途无位置"状态(流式生成会跳过它)。
	if (TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen)) {
		if (*existing) (*existing)->Destroy();
		activeInstances.Remove(citizen);
		spawnTimes.Remove(citizen);
	}
	++latestWalkRequest.FindOrAdd(citizen); // 在途的寻路结果作废
	citizen->EndWalk();
	citizen->SetCurrentRoom(nullptr);
	citizen->ClearPosition();
	trip->phase = Traffic::CommutePhase::Driving;
	trafficFramework->NavigateVehicleToSlot(UTF8_TO_TCHAR(trip->vehicle->GetName().c_str()), trip->parkDest, trip->destSlot);
}

void UForeverPopulaceFrameworkComponent::CancelCommute(Citizen* citizen) {
	Traffic* traffic = GetTrafficCore();
	Traffic::CommuteTrip* trip = traffic ? traffic->FindTrip(citizen) : nullptr;
	if (!trip) return;
	if (trip->phase == Traffic::CommutePhase::WalkToCar) traffic->CancelTrip(citizen); // 车还没出发：释放预订
	else traffic->EndTrip(citizen); // 车已经在路上：它自己开到预订的车位，只是不再有乘客
}

void UForeverPopulaceFrameworkComponent::OnPassengerCarFinished(Vehicle* vehicle, Room* room, bool bArrived) {
	Traffic* traffic = GetTrafficCore();
	if (Traffic::TaskTrip* task = traffic ? traffic->FindTaskByVehicle(vehicle) : nullptr) {
		OnTaskVehicleFinished(task, room, bArrived);
		return;
	}
	Traffic::CommuteTrip* trip = traffic ? traffic->FindTripByVehicle(vehicle) : nullptr;
	if (!trip) return;
	Citizen* citizen = trip->citizen;
	Room* finalDest = trip->finalDest;
	Room* landing = bArrived && room ? room : trip->parkOrigin; // 失败：车没动，市民落回原来的停车房
	if (bArrived) traffic->commuteStats.arrived++;
	else traffic->commuteStats.failed++;
	traffic->EndTrip(citizen);
	if (!citizen || !citizen->IsActive() || !landing) return;

	// 市民在停车房的房间node处出现(没有位置=按房间落点，流式生成会处理)，再普通步行去最终目的地。
	++latestWalkRequest.FindOrAdd(citizen);
	citizen->EndWalk();
	citizen->SetCurrentRoom(landing);
	citizen->ClearPosition();
	if (landing == finalDest) {
		ReportArrival(citizen, nullptr);
		return;
	}
	RequestWalkInternal(citizen, finalDest);
}

void UForeverPopulaceFrameworkComponent::RequestWalkInternal(Citizen* citizen, Room* destination) {
	if (!citizen || !destination || !map) return;
	if (!citizen->IsActive()) return;
	TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen);
	if (existing && *existing && (*existing)->IsPlayerControlled()) return;

	// 跳时间期间：同步算路径，按游戏时间推算(虚拟时钟)，走完时才触发npc_arrive。
	if (bTimeSkipping) {
		StartSkipWalk(citizen, destination);
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
	AsyncPool(GetPathfindingPool(), [weakThis, mapPtr, fromId, toId, citizen, destination, bInTransit, transitFeet, serial]() {
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
	skipWalks.Remove(citizen); // 跳时间里的步行腿被瞬移取代
	// 组织任务：开车途中不受瞬移影响；走路阶段瞬移到别处就中断任务。
	if (Traffic* traffic = GetTrafficCore()) {
		if (Traffic::TaskTrip* task = traffic->FindTaskByWorker(citizen)) {
			if (task->phase == Traffic::TaskPhase::Driving) return;
			Room* expected = task->phase == Traffic::TaskPhase::WalkToVehicle ? task->parkOrigin : task->destRoom;
			if (destination != expected) AbortTaskOfWorker(citizen);
		}
	}
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
		// 去往停车房(通勤第1段)的瞬移照常到达、由ReportArrival上车；其它瞬移取消通勤。
		if (Traffic* traffic = GetTrafficCore()) {
			if (Traffic::CommuteTrip* trip = traffic->FindTrip(citizen); trip && destination != trip->parkOrigin) CancelCommute(citizen);
		}
		++latestWalkRequest.FindOrAdd(citizen); // 在途的寻路结果作废
		Room* previous = citizen->GetCurrentRoom();
		(*existing)->TeleportToRoom(destination);
		ReportArrival(citizen, previous);
		return;
	}

	if (Traffic* traffic = GetTrafficCore()) {
		if (Traffic::CommuteTrip* trip = traffic->FindTrip(citizen); trip && destination != trip->parkOrigin) CancelCommute(citizen);
	}
	++latestWalkRequest.FindOrAdd(citizen); // 在途的寻路结果作废
	Room* previous = citizen->GetCurrentRoom();
	citizen->EndWalk();
	citizen->SetCurrentRoom(destination);
	citizen->ClearPosition();
	ReportArrival(citizen, previous);
}

namespace {
	double PathLengthUU(const TArray<FVector>& points) {
		double length = 0.0;
		for (int32 i = 1; i < points.Num(); i++) length += FVector::Dist(points[i - 1], points[i]);
		return length;
	}

	// 沿路径走fraction(0~1)比例的长度后的位置，并给出之后的第一个路径点下标。
	FVector PointAlongPath(const TArray<FVector>& points, double fraction, int32& outNextIndex) {
		outNextIndex = points.Num();
		if (points.Num() == 0) return FVector::ZeroVector;
		const double target = PathLengthUU(points) * FMath::Clamp(fraction, 0.0, 1.0);
		double walked = 0.0;
		for (int32 i = 1; i < points.Num(); i++) {
			const double segment = FVector::Dist(points[i - 1], points[i]);
			if (walked + segment >= target) {
				outNextIndex = i;
				const double t = segment > KINDA_SMALL_NUMBER ? (target - walked) / segment : 0.0;
				return FMath::Lerp(points[i - 1], points[i], t);
			}
			walked += segment;
		}
		return points.Last();
	}
}

bool UForeverPopulaceFrameworkComponent::GetRoutePath(Citizen* citizen, TArray<FVector>& outPath) const {
	outPath.Reset();
	if (!citizen) return false;
	if (const FSkipWalk* walk = skipWalks.Find(citizen)) {
		int32 nextIndex = 0;
		const double fraction = walk->totalSeconds > 0.0 ? 1.0 - walk->remainingSeconds / walk->totalSeconds : 1.0;
		outPath.Add(PointAlongPath(walk->waypoints, fraction, nextIndex));
		for (int32 i = nextIndex; i < walk->waypoints.Num(); i++) outPath.Add(walk->waypoints[i]);
		return outPath.Num() >= 2;
	}
	if (const TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen); existing && *existing) {
		const ACitizenController* controller = (*existing)->GetCitizenController();
		Room* destination = nullptr;
		return controller && controller->GetRemainingWaypoints(outPath, destination) && outPath.Num() >= 2;
	}
	if (citizen->IsWalking() && GetWorld()) {
		std::vector<Citizen::WalkPoint> remaining;
		citizen->GetRemainingWalk(GetWorld()->GetTimeSeconds(), remaining);
		for (const Citizen::WalkPoint& point : remaining) outPath.Add(FVector(point.x, point.y, point.z) * POPULACE_WORLD_SCALE);
	}
	return outPath.Num() >= 2;
}

double UForeverPopulaceFrameworkComponent::GameSecondsPerRealSecond() const {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	return 60.0 * (player ? player->GetTimeFlowRatio() : 10.0);
}

void UForeverPopulaceFrameworkComponent::RegisterSkipWalk(Citizen* citizen, Room* destination, Room* origin, const TArray<FVector>& waypoints) {
	if (!citizen || !destination) return;
	if (waypoints.Num() < 2) {
		TeleportCitizen(citizen, destination);
		return;
	}
	// 市民从场景里消失，Core里保留"在途无位置"状态(流式生成会跳过它)。
	if (TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen)) {
		if (*existing) (*existing)->Destroy();
		activeInstances.Remove(citizen);
		spawnTimes.Remove(citizen);
	}
	++latestWalkRequest.FindOrAdd(citizen);
	citizen->EndWalk();
	citizen->SetCurrentRoom(nullptr);
	citizen->ClearPosition();

	const ACitizenElement* defaults = GetDefault<ACitizenElement>();
	const double speedUU = FMath::Max(1.0, static_cast<double>(defaults->GetCharacterMovement()->MaxWalkSpeed) * kUnspawnedWalkSpeedScale);
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	FSkipWalk& walk = skipWalks.FindOrAdd(citizen);
	walk.destination = destination;
	walk.origin = origin;
	walk.waypoints = waypoints;
	walk.totalSeconds = PathLengthUU(waypoints) / speedUU * GameSecondsPerRealSecond();
	// 上一条腿在本步里提前走完后多出来的时间，记在这条腿的账上。
	walk.remainingSeconds = walk.totalSeconds - (framework ? framework->GetSkipCreditSeconds() : 0.0);
}

void UForeverPopulaceFrameworkComponent::StartSkipWalk(Citizen* citizen, Room* destination) {
	if (!citizen || !destination || !map || !citizen->IsActive()) return;
	Room* origin = citizen->GetCurrentRoom();
	TArray<FVector> waypoints;
	if (FSkipWalk* old = skipWalks.Find(citizen)) {
		// 上一条腿还没走完：从它当前的位置重新规划(新请求取代旧请求)。
		int32 nextIndex = 0;
		const double fraction = old->totalSeconds > 0.0 ? 1.0 - old->remainingSeconds / old->totalSeconds : 1.0;
		const FVector here = PointAlongPath(old->waypoints, fraction, nextIndex);
		origin = old->origin;
		Map::VehicleRoutePlan plan = map->PlanPedestrianRoute(nullptr, static_cast<float>(here.X / POPULACE_WORLD_SCALE),
			static_cast<float>(here.Y / POPULACE_WORLD_SCALE), destination);
		if (plan.status == Map::VehicleRoutePlan::OK && !plan.points.empty()) {
			waypoints.Add(here);
			for (const Map::NavPoint& point : plan.points) waypoints.Add(FVector(point.x, point.y, point.z) * POPULACE_WORLD_SCALE);
		}
	}
	else {
		if (origin == destination) return;
		Node* fromNode = origin ? origin->GetNavigationNode() : nullptr;
		if (fromNode) {
			const FVector from = FVector(fromNode->GetX(), fromNode->GetY(), fromNode->GetZ()) * POPULACE_WORLD_SCALE;
			PlanWalkPath(citizen, destination, from, waypoints);
		}
	}
	RegisterSkipWalk(citizen, destination, origin, waypoints); // 规划不出来(<2点)按瞬移到达
}

void UForeverPopulaceFrameworkComponent::DecrementSkipLegs(double stepGameSeconds) {
	for (TPair<Citizen*, FSkipWalk>& pair : skipWalks) pair.Value.remainingSeconds -= stepGameSeconds;
}

bool UForeverPopulaceFrameworkComponent::CompleteSkipLegs() {
	TArray<Citizen*> due;
	for (const TPair<Citizen*, FSkipWalk>& pair : skipWalks) {
		if (pair.Value.remainingSeconds <= 0.0) due.Add(pair.Key);
	}
	if (due.Num() == 0) return false;
	due.Sort([](const Citizen& a, const Citizen& b) { return a.GetName() < b.GetName(); }); // TArray<T*>::Sort的谓词收到解引用后的元素 // 同一步多条腿同时完成：按姓名稳定排序
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	for (Citizen* citizen : due) {
		FSkipWalk* walk = skipWalks.Find(citizen);
		if (!walk || walk->remainingSeconds > 0.0) continue; // 处理前面的到达时被取代/重新规划了
		Room* destination = walk->destination;
		Room* origin = walk->origin;
		const double credit = -walk->remainingSeconds;
		skipWalks.Remove(citizen);
		if (!citizen->IsActive()) continue;
		if (framework) framework->SetSkipCreditSeconds(credit);
		++latestWalkRequest.FindOrAdd(citizen);
		citizen->EndWalk();
		citizen->SetCurrentRoom(destination);
		citizen->ClearPosition();
		ReportArrival(citizen, origin);
	}
	if (framework) framework->SetSkipCreditSeconds(0.0);
	return true;
}

void UForeverPopulaceFrameworkComponent::EndTimeSkip() {
	bTimeSkipping = false;
	TArray<Citizen*> keys;
	skipWalks.GetKeys(keys);
	for (Citizen* citizen : keys) {
		FSkipWalk walk = skipWalks.FindRef(citizen);
		skipWalks.Remove(citizen);
		if (!citizen || !citizen->IsActive()) continue;
		int32 nextIndex = 0;
		const double fraction = walk.totalSeconds > 0.0 ? 1.0 - walk.remainingSeconds / walk.totalSeconds : 1.0;
		TArray<FVector> rest;
		rest.Add(PointAlongPath(walk.waypoints, fraction, nextIndex));
		for (int32 i = nextIndex; i < walk.waypoints.Num(); i++) rest.Add(walk.waypoints[i]);
		ApplyWalkResult(citizen, walk.destination, rest); // 落成真实步行：没有Actor就Core推算，进入生成距离再生成
	}
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
		TArray<FVector> remaining;
		if (TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen); existing && *existing) {
			if ((*existing)->IsPlayerControlled()) continue;
			ACitizenController* controller = (*existing)->GetCitizenController();
			if (controller) controller->GetRemainingWaypoints(remaining, destination);
		}
		else if (citizen->IsWalking()) {
			destination = citizen->GetWalkDestination();
			std::vector<Citizen::WalkPoint> remainingCore;
			citizen->GetRemainingWalk(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, remainingCore);
			for (const Citizen::WalkPoint& point : remainingCore) remaining.Add(FVector(point.x, point.y, point.z) * POPULACE_WORLD_SCALE);
		}
		// 在途的行走转成虚拟时钟上的腿(从当前位置接着走)，没有剩余路径(没拿到)才直接到达。
		if (destination) {
			if (remaining.Num() >= 2) RegisterSkipWalk(citizen, destination, citizen->GetCurrentRoom(), remaining);
			else TeleportCitizen(citizen, destination);
		}
	}
}

void UForeverPopulaceFrameworkComponent::DrainPathResults() {
	FPendingWalkResult result;
	const double drainStart = FPlatformTime::Seconds();
	int32 applied = 0;
	while (pathResultQueue.Dequeue(result)) {
		const uint32* latest = latestWalkRequest.Find(result.citizen);
		if (!latest || *latest != result.serial) continue; // 已被更新的导航/瞬移取代
		ApplyWalkResult(result.citizen, result.destination, result.waypoints);
		applied++;
	}
	// 卡顿排查：一帧里落地寻路结果超过10ms就打日志。
	const double drainSeconds = FPlatformTime::Seconds() - drainStart;
	if (drainSeconds > 0.01) {
		UE_LOG(LogTemp, Warning, TEXT("DrainPathResults: applied %d results in %.1f ms."), applied, drainSeconds * 1000.0);
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
	pathfindingPool.Reset(); // 下一局第一次寻路时重建
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
	if (!citizen || !citizen->IsActive()) return;
	// 组织任务：走到了组织的车所在的停车房——不是真正的到达(不广播npc_arrive)，上车；走进了目的房间——任务完成，
	// 下面照常广播npc_arrive之后再给组织脚本发task_complete；走到了别处——任务中断。
	bool bTaskComplete = false;
	std::string taskLabel, taskOrigin, taskDestination;
	if (Traffic* traffic = GetTrafficCore()) {
		if (Traffic::TaskTrip* task = traffic->FindTaskByWorker(citizen)) {
			Room* current = citizen->GetCurrentRoom();
			if (task->phase == Traffic::TaskPhase::WalkToVehicle && current == task->parkOrigin) {
				BoardTruck(citizen); // 走到了组织的车：不是真正的到达，不广播npc_arrive
				return;
			}
			if (task->phase == Traffic::TaskPhase::WalkOut && current == task->destRoom) {
				bTaskComplete = true;
				taskLabel = task->task;
				taskOrigin = task->originRoom ? task->originRoom->GetAddress() : std::string();
				taskDestination = task->destRoom->GetAddress();
				traffic->EndTask(task); // 单程到此结束，往返由组织脚本再派任务
			}
			else if (task->phase != Traffic::TaskPhase::Driving) {
				AbortTaskOfWorker(citizen); // 走到了别处：任务中断
			}
		}
		if (Traffic::CommuteTrip* trip = traffic->FindTrip(citizen); trip && trip->phase == Traffic::CommutePhase::WalkToCar) {
			if (citizen->GetCurrentRoom() == trip->parkOrigin) {
				BoardCar(citizen);
				return;
			}
			CancelCommute(citizen);
		}
	}
	if (citizen->GetCurrentRoom() == previousRoom) {
		if (bTaskComplete) NotifyTaskResult(citizen, taskLabel, taskDestination, taskOrigin, nullptr);
		return;
	}
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (UForeverStoryFrameworkComponent* story = framework ? framework->GetStoryFramework() : nullptr) {
		story->BroadcastNpcArrive(citizen, citizen->GetCurrentRoom());
	}
	if (bTaskComplete) NotifyTaskResult(citizen, taskLabel, taskDestination, taskOrigin, nullptr);
}

void UForeverPopulaceFrameworkComponent::NotifyTaskResult(Citizen* worker, const std::string& task, const std::string& destination,
	const std::string& origin, const char* failReason) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	UForeverStoryFrameworkComponent* story = framework ? framework->GetStoryFramework() : nullptr;
	if (!story || !worker || !worker->GetJob() || !framework->GetSociety()) return;
	Organization* organization = framework->GetSociety()->FindOrganizationOfJob(worker->GetJob());
	if (!organization) return;
	if (failReason) story->BroadcastTaskFailed(organization, task, destination, worker->GetName(), origin, failReason);
	else story->BroadcastTaskComplete(organization, task, destination, worker->GetName(), origin);
}

bool UForeverPopulaceFrameworkComponent::AssignTask(Organization* organization, const std::string& task, Room* destination,
	const std::vector<std::string>& categories, const std::string& workerName) {
	Traffic* traffic = GetTrafficCore();
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (!traffic || !organization || !destination || !framework) return false;

	// 没有交通框架(比如测试里)就没法开车，只留走路那几项。
	std::vector<std::string> usable;
	for (const std::string& category : categories) {
		if (category.empty() || framework->GetTrafficFramework()) usable.push_back(category);
	}

	auto available = [this](const Citizen* candidate) {
		TObjectPtr<ACitizenElement>* element = activeInstances.Find(const_cast<Citizen*>(candidate));
		return !(element && *element && (*element)->IsPlayerControlled());
	};
	std::string failReason;
	Traffic::TaskTrip* trip = traffic->BeginTask(organization, task, destination, usable, workerName, available, &failReason);
	if (!trip) {
		UE_LOG(LogTemp, Log, TEXT("assign_task: %s -> %s not dispatched (%s)."), UTF8_TO_TCHAR(task.c_str()),
			UTF8_TO_TCHAR(destination->GetAddress().c_str()), UTF8_TO_TCHAR(failReason.c_str()));
		if (UForeverStoryFrameworkComponent* story = framework->GetStoryFramework()) {
			story->BroadcastTaskFailed(organization, task, destination->GetAddress(), workerName, std::string(), failReason);
		}
		return false;
	}
	Citizen* worker = trip->worker;
	Room* originRoom = trip->originRoom;
	Room* parkOrigin = trip->parkOrigin;
	const bool bDrive = trip->vehicle != nullptr;
	UE_LOG(LogTemp, Log, TEXT("assign_task: %s -> %s (%s) by %s"), UTF8_TO_TCHAR(task.c_str()), UTF8_TO_TCHAR(destination->GetAddress().c_str()),
		bDrive ? TEXT("drive") : TEXT("walk"), UTF8_TO_TCHAR(worker->GetName().c_str()));

	if (UForeverStoryFrameworkComponent* story = framework->GetStoryFramework()) {
		story->BroadcastExecuteTask(worker, task, destination->GetAddress());
	}
	// 事件脚本里可能又改了行程(极少)：以名字重新取，找不到就不再往下。
	if (traffic->FindTaskByWorker(worker) != trip) return true;
	if (bDrive) {
		if (originRoom == parkOrigin) BoardTruck(worker);
		else RequestWalkInternal(worker, parkOrigin);
	}
	else {
		RequestWalkInternal(worker, destination);
	}
	return true;
}

void UForeverPopulaceFrameworkComponent::BoardTruck(Citizen* citizen) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Traffic* traffic = GetTrafficCore();
	UForeverTrafficFrameworkComponent* trafficFramework = framework ? framework->GetTrafficFramework() : nullptr;
	Traffic::TaskTrip* trip = traffic ? traffic->FindTaskByWorker(citizen) : nullptr;
	if (!trip || !trafficFramework || !trip->vehicle) {
		AbortTaskOfWorker(citizen);
		return;
	}
	if (TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen)) {
		if (*existing) (*existing)->Destroy();
		activeInstances.Remove(citizen);
		spawnTimes.Remove(citizen);
	}
	++latestWalkRequest.FindOrAdd(citizen);
	citizen->EndWalk();
	citizen->SetCurrentRoom(nullptr);
	citizen->ClearPosition();
	trip->phase = Traffic::TaskPhase::Driving;
	trafficFramework->NavigateVehicleToSlot(UTF8_TO_TCHAR(trip->vehicle->GetName().c_str()), trip->parkDest, trip->destSlot);
}

void UForeverPopulaceFrameworkComponent::FinishTaskLanding(Traffic::TaskTrip* trip, Room* landing, const char* failReason) {
	Traffic* traffic = GetTrafficCore();
	if (!trip || !traffic) return;
	Citizen* worker = trip->worker;
	if (!worker || !worker->IsActive() || !landing) {
		traffic->EndTask(trip);
		return;
	}
	trip->vehicle = nullptr; // 车已经停好(或根本没动)，不再属于这个行程
	++latestWalkRequest.FindOrAdd(worker);
	worker->EndWalk();
	worker->SetCurrentRoom(landing);
	worker->ClearPosition();
	if (failReason) {
		// 车没能开成：员工落回出发的停车房，任务失败，由组织脚本决定怎么办。
		const std::string task = trip->task;
		const std::string origin = trip->originRoom ? trip->originRoom->GetAddress() : std::string();
		const std::string destination = trip->destRoom ? trip->destRoom->GetAddress() : std::string();
		traffic->EndTask(trip);
		ReportArrival(worker, nullptr);
		NotifyTaskResult(worker, task, destination, origin, failReason);
		return;
	}
	trip->phase = Traffic::TaskPhase::WalkOut;
	if (landing == trip->destRoom) {
		ReportArrival(worker, nullptr); // 目的房间就在停车的那个房间：直接完成
		return;
	}
	RequestWalkInternal(worker, trip->destRoom);
}

void UForeverPopulaceFrameworkComponent::OnTaskVehicleFinished(Traffic::TaskTrip* trip, Room* room, bool bArrived) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Traffic* traffic = GetTrafficCore();
	Vehicle* vehicle = trip->vehicle;
	if (!traffic || !framework || !framework->GetTrafficFramework() || !vehicle) {
		if (traffic) traffic->EndTask(trip);
		return;
	}
	if (trip->phase != Traffic::TaskPhase::Driving) return;

	if (bArrived) {
		// 车停在了目的车位：员工在那里下车，走向目的房间。
		FinishTaskLanding(trip, room ? room : trip->parkDest, nullptr);
		return;
	}
	// 车没能出发/开不到：还停在原车位，员工落回停车房，任务失败。
	vehicle->ClearReservation();
	UE_LOG(LogTemp, Warning, TEXT("assign_task: truck navigation failed, worker lands back at the origin."));
	FinishTaskLanding(trip, trip->parkOrigin, "navigation");
}

void UForeverPopulaceFrameworkComponent::OnTaskVehicleTaken(Vehicle* vehicle) {
	Traffic* traffic = GetTrafficCore();
	Traffic::TaskTrip* trip = traffic ? traffic->FindTaskByVehicle(vehicle) : nullptr;
	if (!trip) return;
	Citizen* worker = trip->worker;
	const std::string task = trip->task;
	const std::string origin = trip->originRoom ? trip->originRoom->GetAddress() : std::string();
	const std::string destination = trip->destRoom ? trip->destRoom->GetAddress() : std::string();
	if (trip->phase == Traffic::TaskPhase::Driving) {
		// 员工已经在车里：他在原来的停车房下车，任务失败(车现在归玩家开了)。
		FinishTaskLanding(trip, trip->parkOrigin, "vehicle_taken");
		return;
	}
	traffic->CancelTask(trip); // 员工还在走向车：任务取消，他继续走到停车房就是了
	NotifyTaskResult(worker, task, destination, origin, "vehicle_taken");
}

void UForeverPopulaceFrameworkComponent::AbortTaskOfWorker(Citizen* citizen) {
	Traffic* traffic = GetTrafficCore();
	Traffic::TaskTrip* trip = traffic ? traffic->FindTaskByWorker(citizen) : nullptr;
	if (!trip) return;
	if (trip->phase == Traffic::TaskPhase::Driving) {
		trip->worker = nullptr; // 车仍要自己开到站，只是不再有员工
		if (citizen && citizen->GetJob() && citizen->GetJob()->GetScript()) citizen->GetJob()->GetScript()->SetValue("task", ValueType(std::string()));
		return;
	}
	traffic->CancelTask(trip);
}

void UForeverPopulaceFrameworkComponent::HandleElementEndPlay(Citizen* citizen, ACitizenElement* element) {
	if (!citizen) return;
	TObjectPtr<ACitizenElement>* existing = activeInstances.Find(citizen);
	if (!existing || existing->Get() != element) return;
	activeInstances.Remove(citizen);
	spawnTimes.Remove(citizen);
}

void UForeverPopulaceFrameworkComponent::HandleCitizenDeath(Citizen* citizen) {
	AbortTaskOfWorker(citizen); // 开着车的任务让车自己开到站，其它取消
	skipWalks.Remove(citizen);
	if (!citizen) return;
	CancelCommute(citizen);

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

bool UForeverPopulaceFrameworkComponent::HandBackCitizenToCore(Citizen* citizen, ACitizenElement* element, double now, bool bForce) {
	// 先把当前位置转回地图单位写回Citizen(这样下次重新进入范围直接用这份记录，不再随机抖动一次)，再销毁Actor。
	FVector loc = element->GetActorLocation();
	// 还在走路：剩余路径交还Core继续按时间推算，不然目的地就丢了(currentRoom还是出发地)。
	TArray<FVector> remainingPath;
	Room* walkDestination = nullptr;
	ACitizenController* controller = element->GetCitizenController();
	const bool bHasPath = controller && controller->GetRemainingWaypoints(remainingPath, walkDestination) && remainingPath.Num() > 1;
	if (bForce && !bHasPath) return false; // 脱困必须能接着走，没拿到路径就不动它
	citizen->SetPosition(loc.X / POPULACE_WORLD_SCALE, loc.Y / POPULACE_WORLD_SCALE, loc.Z / POPULACE_WORLD_SCALE);
	if (bHasPath) {
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
	spawnTimes.Remove(citizen);
	stuckStates.Remove(citizen);
	return true;
}

void UForeverPopulaceFrameworkComponent::CheckStuckCitizens(double now) {
	if (stuckTimeoutSeconds <= 0.f) return;
	const double realNow = FPlatformTime::Seconds();
	TArray<Citizen*> toHandBack;
	for (const auto& [citizen, element] : activeInstances) {
		if (!citizen || !element || element->IsPlayerControlled() || element->IsDespawnExempt()) { stuckStates.Remove(citizen); continue; }
		const ACitizenController* controller = element->GetCitizenController();
		if (!controller || !controller->HasCommands()) { stuckStates.Remove(citizen); continue; } // 没在走路：不算卡住
		const FVector location = element->GetActorLocation();
		FStuckState& state = stuckStates.FindOrAdd(citizen);
		if (state.sinceReal == 0.0 || FVector::Dist(location, state.anchor) > stuckMoveThreshold) {
			state.anchor = location;
			state.sinceReal = realNow;
			continue;
		}
		if (realNow - state.sinceReal >= stuckTimeoutSeconds) toHandBack.Add(citizen);
	}
	for (Citizen* citizen : toHandBack) {
		TObjectPtr<ACitizenElement>* found = activeInstances.Find(citizen);
		if (!found || !*found) continue;
		UE_LOG(LogTemp, Warning, TEXT("脱困：市民 %s 超过%.0f秒没有前进，强制流式销毁并交回Core推算。"),
			UTF8_TO_TCHAR(citizen->GetName().c_str()), stuckTimeoutSeconds);
		if (HandBackCitizenToCore(citizen, *found, now, true)) {
			respawnBlockedUntil.Add(citizen, now + 30.0);
		}
		else if (FStuckState* state = stuckStates.Find(citizen)) {
			state->sinceReal = FPlatformTime::Seconds(); // 拿不到路径：再等一个周期
		}
	}
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

	// 暂停/时间冻结时不计卡住。
	if (DeltaTime > 0.f && GetWorld() && !GetWorld()->IsPaused()) CheckStuckCitizens(GetWorld()->GetTimeSeconds());
	else stuckStates.Reset();

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
			if (const double* blockedUntil = respawnBlockedUntil.Find(citizen)) {
				if (now < *blockedUntil) continue; // 刚脱困：先让Core带着它走远一段
				respawnBlockedUntil.Remove(citizen);
			}
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
				HandBackCitizenToCore(citizen, element, now, false);
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
