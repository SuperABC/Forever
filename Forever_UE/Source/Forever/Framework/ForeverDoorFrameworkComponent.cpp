#include "Framework/ForeverDoorFrameworkComponent.h"

#include "Element/DoorComponent.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"

#include "Kismet/GameplayStatics.h"

#include "map/door.h"
#include "map/map.h"

#define DOOR_WORLD_SCALE 1000.f

UForeverDoorFrameworkComponent::UForeverDoorFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = true;
}

bool UForeverDoorFrameworkComponent::IsWorldGenerationDone() const {
	const AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(GetOwner());
	return frameworkActor == nullptr || frameworkActor->IsInitialGenerationDone();
}

FIntPoint UForeverDoorFrameworkComponent::CellOf(const FVector& location) const {
	return FIntPoint(FMath::FloorToInt(location.X / kGridCellSize), FMath::FloorToInt(location.Y / kGridCellSize));
}

void UForeverDoorFrameworkComponent::GenerateDoors(Map* inMap) {
	map = inMap;
	entries.Reset();
	grid.Reset();
	activeEntries.Reset();
	if (!map) return;

	const std::vector<Door*>& doors = map->GetAllDoors();
	entries.Reserve(static_cast<int32>(doors.size()));
	for (Door* door : doors) {
		if (!door) continue;
		FDoorEntry entry;
		entry.door = door;
		// Door::GetZ是门洞底边中点，取门高一半作为中心，垂直距离按门洞中心算。
		entry.location = FVector(door->GetX() * DOOR_WORLD_SCALE, door->GetY() * DOOR_WORLD_SCALE,
			(door->GetZ() + door->GetHeight() * 0.5f) * DOOR_WORLD_SCALE);
		int32 index = entries.Add(entry);
		grid.FindOrAdd(CellOf(entry.location)).Add(index);
	}
}

void UForeverDoorFrameworkComponent::SpawnDoor(FDoorEntry& entry) {
	AActor* owner = GetOwner();
	if (!owner || !entry.door) return;

	UForeverDoorComponent* comp = NewObject<UForeverDoorComponent>(owner, NAME_None, RF_Transient);
	comp->SetupAttachment(owner->GetRootComponent());
	comp->RegisterComponent();
	comp->Init(entry.door);
	owner->AddInstanceComponent(comp);
	entry.component = comp;
}

void UForeverDoorFrameworkComponent::DespawnDoor(FDoorEntry& entry) {
	UForeverDoorComponent* comp = entry.component.Get();
	entry.component.Reset();
	if (!comp) return;
	// 与原来建筑销毁近景LOD门的流程一致：先置空Door*(Tick/Overlap回调判空)，再摘掉实例组件并销毁。
	comp->ClearDoor();
	if (AActor* owner = GetOwner()) owner->RemoveInstanceComponent(comp);
	comp->DestroyComponent();
}

void UForeverDoorFrameworkComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!map || entries.IsEmpty() || !IsWorldGenerationDone()) return;

	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn) return;
	const FVector playerLoc = pawn->GetActorLocation();

	// 距离跟随市民流式加载：用"移动中"的那一档(静止市民的 spawn/despawn 乘 movingDistanceScale)，
	// 因为走路的市民会比静止的更早出现，门要在他们之前就位；度量与市民扫描完全相同(Z差放大)。
	float spawnDistance = kFallbackSpawnDistance;
	float despawnDistance = kFallbackDespawnDistance;
	if (const AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(GetOwner())) {
		if (const UForeverPopulaceFrameworkComponent* populace = frameworkActor->GetPopulaceFramework()) {
			spawnDistance = populace->GetCitizenSpawnDistance() * populace->GetMovingDistanceScale();
			despawnDistance = populace->GetCitizenDespawnDistance() * populace->GetMovingDistanceScale();
		}
	}
	const float spawnLimit = spawnDistance * DOOR_WORLD_SCALE;
	const float despawnLimit = despawnDistance * DOOR_WORLD_SCALE;

	// 1) 销毁：遍历已生成的门，超出销毁距离才销毁(销毁距离大于生成距离，形成迟滞区)，每帧限量。
	int32 despawned = 0;
	for (auto it = activeEntries.CreateIterator(); it; ++it) {
		FDoorEntry& entry = entries[*it];
		if (!entry.component.IsValid()) { it.RemoveCurrent(); continue; }
		if (despawned >= FMath::Max(1, maxDespawnsPerFrame)) break;
		if (UForeverPopulaceFrameworkComponent::StreamingDistanceBetween(playerLoc, entry.location) > despawnLimit) {
			DespawnDoor(entry);
			it.RemoveCurrent();
			despawned++;
		}
	}

	// 2) 生成：只查生成距离覆盖到的格子(水平距离 <= 加权距离，所以半径覆盖足够)，近的优先，每帧限量。
	spawnCandidates.Reset();
	const FIntPoint center = CellOf(playerLoc);
	const int32 cellRadius = FMath::CeilToInt(spawnLimit / kGridCellSize);
	for (int32 dx = -cellRadius; dx <= cellRadius; dx++) {
		for (int32 dy = -cellRadius; dy <= cellRadius; dy++) {
			const TArray<int32>* cell = grid.Find(FIntPoint(center.X + dx, center.Y + dy));
			if (!cell) continue;
			for (int32 index : *cell) {
				if (activeEntries.Contains(index)) continue;
				const float dist = UForeverPopulaceFrameworkComponent::StreamingDistanceBetween(playerLoc, entries[index].location);
				if (dist < spawnLimit) spawnCandidates.Emplace(dist, index);
			}
		}
	}
	if (spawnCandidates.IsEmpty()) return;
	spawnCandidates.Sort([](const TPair<float, int32>& a, const TPair<float, int32>& b) { return a.Key < b.Key; });
	int32 spawned = 0;
	for (const TPair<float, int32>& candidate : spawnCandidates) {
		if (spawned >= FMath::Max(1, maxSpawnsPerFrame)) break;
		FDoorEntry& entry = entries[candidate.Value];
		SpawnDoor(entry);
		if (entry.component.IsValid()) activeEntries.Add(candidate.Value);
		spawned++;
	}
}
