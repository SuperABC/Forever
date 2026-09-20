#include "Framework/ForeverPopulaceFrameworkComponent.h"

#include "Element/CitizenElement.h"

#include "Kismet/GameplayStatics.h"

#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "map/geometry.h"
#include "populace/populace.h"
#include "populace/citizen.h"

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

	TArray<FVector> waypoints;
	if (fromNode && toNode) {
		std::vector<const Node*> path = map->FindPedestrianPath(fromNode->GetId(), toNode->GetId());
		for (const Node* node : path) {
			waypoints.Add(FVector(node->GetX(), node->GetY(), node->GetZ()) * POPULACE_WORLD_SCALE);
		}
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
			if (FVector::Dist(playerLoc, element->GetActorLocation()) > despawnDistUE) {
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
