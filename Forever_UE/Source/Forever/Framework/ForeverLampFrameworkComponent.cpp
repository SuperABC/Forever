#include "Framework/ForeverLampFrameworkComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#include "Element/LampElement.h"
#include "common/utility.h"
#include "map/map.h"

#define LAMP_WORLD_SCALE 1000.f

using namespace std;

UForeverLampFrameworkComponent::UForeverLampFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = false;
}

void UForeverLampFrameworkComponent::GenerateLamps(Map* inMap) {
	map = inMap;
	if (!map) return;

	const Lamps& lamps = map->GetLamps();
	const auto& blueprintPaths = lamps.GetBlueprintPaths();

	// 蓝图类一次性加载，Core的blueprintIndex -> 这里的UClass。
	TArray<UClass*> classes;
	for (const string& path : blueprintPaths) {
		classes.Add(GetOrLoadClass(UTF8_TO_TCHAR(path.c_str())));
	}

	int32 spawned = 0;
	for (const LampInstance& lamp : lamps.GetLamps()) {
		nextLampId = FMath::Max(nextLampId, lamp.id + 1);
		if (lamp.blueprintIndex < 0 || lamp.blueprintIndex >= classes.Num()) continue;
		UClass* lampClass = classes[lamp.blueprintIndex];
		if (!lampClass) continue;
		if (ALampElement* actor = SpawnLamp(lampClass, lamp.x, lamp.y, lamp.yaw)) {
			lampActors.Add(lamp.id, actor);
			spawned++;
		}
	}
	// 新灯都还没推过亮度，下一次UpdateLamps必须重新同步。
	currentBrightness = -1.f;
	UE_LOG(LogTemp, Log, TEXT("UForeverLampFrameworkComponent: %d blueprints, %d lamps."), classes.Num(), spawned);
}

int32 UForeverLampFrameworkComponent::AddLamp(const FString& blueprintPath, float mapX, float mapY, float yaw) {
	UClass* lampClass = GetOrLoadClass(blueprintPath);
	if (!lampClass) return -1;
	ALampElement* actor = SpawnLamp(lampClass, mapX, mapY, yaw);
	if (!actor) return -1;
	int32 lampId = nextLampId++;
	lampActors.Add(lampId, actor);
	if (currentBrightness >= 0.f) actor->ApplyBrightness(currentBrightness);
	return lampId;
}

bool UForeverLampFrameworkComponent::RemoveLamp(int32 lampId) {
	TObjectPtr<ALampElement> actor;
	if (!lampActors.RemoveAndCopyValue(lampId, actor)) return false;
	if (actor) actor->Destroy();
	return true;
}

void UForeverLampFrameworkComponent::UpdateLamps(const Time& gameTime, float deltaTime) {
	int hour = gameTime.GetHour();
	bool night = hour >= onHour || hour < offHour;
	float target = night ? 1.f : 0.f;
	if (target != currentBrightness) {
		currentBrightness = target;
		for (auto& [lampId, actor] : lampActors) {
			if (actor) actor->ApplyBrightness(target);
		}
	}

	nearFarTimer += deltaTime;
	if (nearFarTimer >= updateInterval) {
		nearFarTimer = 0.f;
		UpdateNearFar();
	}
}

void UForeverLampFrameworkComponent::UpdateNearFar() {
	UWorld* world = GetWorld();
	if (!world) return;
	// 用相机位置：第一人称/载具/第三人称都以玩家看到的位置为准；没有相机管理器(还没有玩家)时不切换。
	APlayerCameraManager* camera = UGameplayStatics::GetPlayerCameraManager(world, 0);
	if (!camera) return;
	FVector viewer = camera->GetCameraLocation();

	float nearSquared = nearDistance * nearDistance;
	float farSquared = farDistance * farDistance;
	for (auto& [lampId, actor] : lampActors) {
		if (!actor) continue;
		float distanceSquared = FVector::DistSquared(viewer, actor->GetActorLocation());
		if (!actor->IsNear() && distanceSquared < nearSquared) {
			actor->SwitchNear();
		}
		else if (actor->IsNear() && distanceSquared > farSquared) {
			actor->SwitchFar();
		}
	}
}

UClass* UForeverLampFrameworkComponent::GetOrLoadClass(const FString& blueprintPath) {
	if (TObjectPtr<UClass>* found = classByPath.Find(blueprintPath)) return *found;
	if (failedPaths.Contains(blueprintPath)) return nullptr;

	UClass* lampClass = LoadClass<ALampElement>(nullptr, *blueprintPath);
	if (!lampClass) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverLampFrameworkComponent: failed to load lamp blueprint %s."), *blueprintPath);
		failedPaths.Add(blueprintPath);
		return nullptr;
	}
	classByPath.Add(blueprintPath, lampClass);
	return lampClass;
}

ALampElement* UForeverLampFrameworkComponent::SpawnLamp(UClass* lampClass, float mapX, float mapY, float yaw) {
	UWorld* world = GetWorld();
	if (!world || !map) return nullptr;

	FVector location(mapX * LAMP_WORLD_SCALE, mapY * LAMP_WORLD_SCALE, map->SampleHeight(mapX, mapY) * LAMP_WORLD_SCALE);
	FRotator rotation(0.0, FMath::RadiansToDegrees(yaw), 0.0);
	FActorSpawnParameters params;
	params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return world->SpawnActor<ALampElement>(lampClass, location, rotation, params);
}
