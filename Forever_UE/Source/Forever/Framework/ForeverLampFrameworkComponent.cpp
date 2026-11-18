#include "Framework/ForeverLampFrameworkComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
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
			RegisterLampMesh(lamp.id, actor, /*batch=*/true);
			spawned++;
		}
	}
	CommitLampInstances();
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
	RegisterLampMesh(lampId, actor, /*batch=*/false);
	if (currentBrightness >= 0.f) actor->ApplyBrightness(currentBrightness);
	return lampId;
}

bool UForeverLampFrameworkComponent::RemoveLamp(int32 lampId) {
	TObjectPtr<ALampElement> actor;
	if (!lampActors.RemoveAndCopyValue(lampId, actor)) return false;
	if (actor) actor->Destroy();
	// ISM里的实例不真删(删除会重排其它实例的下标)，缩成0藏起来；下标不复用。
	if (TArray<TPair<int32, int32>>* instances = lampInstances.Find(lampId)) {
		const FTransform hidden(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
		for (const TPair<int32, int32>& instance : *instances) {
			if (meshParts.IsValidIndex(instance.Key) && meshParts[instance.Key].ism) {
				meshParts[instance.Key].ism->UpdateInstanceTransform(instance.Value, hidden, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true);
			}
		}
		lampInstances.Remove(lampId);
	}
	return true;
}

void UForeverLampFrameworkComponent::UpdateLamps(const Time& gameTime, float deltaTime) {
	int hour = gameTime.GetHour();
	bool night = hour >= onHour || hour < offHour;
	float target = night ? 1.f : 0.f;
	if (target != currentBrightness) {
		currentBrightness = target;
		ApplyEmissive(target);
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

void UForeverLampFrameworkComponent::RegisterLampMesh(int32 lampId, ALampElement* actor, bool batch) {
	AActor* owner = GetOwner();
	if (!owner || !actor) return;

	TInlineComponentArray<UStaticMeshComponent*> components;
	actor->GetComponents(components);
	for (UStaticMeshComponent* component : components) {
		UStaticMesh* mesh = component->GetStaticMesh();
		if (!mesh || component->IsA<UInstancedStaticMeshComponent>()) continue;

		const FString key = FString::Printf(TEXT("%s#%s"), *actor->GetClass()->GetPathName(), *component->GetName());
		int32 partIndex = INDEX_NONE;
		if (int32* found = partByKey.Find(key)) {
			partIndex = *found;
		}
		else {
			// 第一次见到这种灯的这个网格组件：照它的网格/材质建ISM。蓝图BeginPlay已把灯头槽换成了该Actor自己的动态材质，换回它的父材质。
			// 参数来自这种灯的基类ALampElement::meshSettings(蓝图Class Defaults)。
			const FLampMeshSettings& settings = actor->meshSettings;
			FLampMeshPart part;
			part.emissiveParameter = settings.emissiveParameterName;
			part.emissiveOn = settings.emissiveStrengthOn;
			part.emissiveOff = settings.emissiveStrengthOff;
			UInstancedStaticMeshComponent* ism = NewObject<UInstancedStaticMeshComponent>(owner, NAME_None, RF_Transient);
			ism->SetStaticMesh(mesh);
			ism->SetupAttachment(owner->GetRootComponent());
			ism->SetMobility(EComponentMobility::Static);
			ism->SetGenerateOverlapEvents(false);
			ism->SetCastShadow(settings.castShadow);
			if (!settings.hasCollision) ism->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 有碰撞时保持ISM默认
			for (int32 slot = 0; slot < component->GetNumMaterials(); slot++) {
				UMaterialInterface* material = component->GetMaterial(slot);
				if (UMaterialInstanceDynamic* dynamic = Cast<UMaterialInstanceDynamic>(material)) material = dynamic->Parent;
				if (material && slot == settings.emissiveMaterialSlot) {
					part.emissive = UMaterialInstanceDynamic::Create(material, ism);
					part.emissive->SetScalarParameterValue(part.emissiveParameter,
						FMath::Lerp(part.emissiveOff, part.emissiveOn, FMath::Clamp(currentBrightness, 0.f, 1.f)));
					material = part.emissive;
				}
				ism->SetMaterial(slot, material);
			}
			if (settings.cullDistance > 0.f) ism->SetCullDistances(0, FMath::RoundToInt(settings.cullDistance));
			ism->RegisterComponent();
			owner->AddInstanceComponent(ism);
			part.ism = ism;
			partIndex = meshParts.Add(part);
			partByKey.Add(key, partIndex);
		}

		FLampMeshPart& part = meshParts[partIndex];
		const FTransform transform = component->GetComponentTransform();
		int32 instanceIndex;
		if (batch) {
			instanceIndex = part.ism->GetInstanceCount() + part.pending.Num();
			part.pending.Add(transform);
		}
		else {
			instanceIndex = part.ism->AddInstance(transform, /*bWorldSpace=*/true);
		}
		lampInstances.FindOrAdd(lampId).Add(TPair<int32, int32>(partIndex, instanceIndex));

		// Actor自己的网格不再渲染、不要碰撞(碰撞由ISM承担)。
		component->SetVisibility(false);
		component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void UForeverLampFrameworkComponent::CommitLampInstances() {
	int64 total = 0;
	for (FLampMeshPart& part : meshParts) {
		if (!part.ism || part.pending.IsEmpty()) continue;
		part.ism->AddInstances(part.pending, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);
		total += part.pending.Num();
		part.pending.Empty();
	}
	UE_LOG(LogTemp, Log, TEXT("UForeverLampFrameworkComponent: %d lamp mesh ISMs, %lld instances."), meshParts.Num(), total);
}

void UForeverLampFrameworkComponent::ApplyEmissive(float brightness) {
	for (FLampMeshPart& part : meshParts) {
		if (part.emissive) {
			part.emissive->SetScalarParameterValue(part.emissiveParameter,
				FMath::Lerp(part.emissiveOff, part.emissiveOn, FMath::Clamp(brightness, 0.f, 1.f)));
		}
	}
}
