#include "Framework/ForeverVegetationFrameworkComponent.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#include "map/map.h"

#define VEGETATION_WORLD_SCALE 1000.f

using namespace std;

UForeverVegetationFrameworkComponent::UForeverVegetationFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = false;
}

void UForeverVegetationFrameworkComponent::GenerateVegetation(Map* inMap) {
	map = inMap;
	if (!map) return;

	const Vegetation& vegetation = map->GetVegetation();
	const auto& meshPaths = vegetation.GetMeshPaths();
	const auto& trees = vegetation.GetTrees();

	// 第一步：所有mesh一次性加载、每个mesh建好一个HISM。Core的meshIndex -> 这里的下标。
	TArray<int32> localMeshIndex;
	localMeshIndex.SetNum(static_cast<int32>(meshPaths.size()));
	for (int32 i = 0; i < localMeshIndex.Num(); i++) {
		localMeshIndex[i] = GetOrCreateMesh(UTF8_TO_TCHAR(meshPaths[i].c_str()));
	}

	// 第二步：按mesh分组，每个HISM只调一次AddInstances(批量添加比逐个AddInstance快得多，
	// 而且只触发一次树重建)。
	TArray<TArray<FTransform>> transforms;
	TArray<TArray<int32>> treeIds;
	transforms.SetNum(meshComponents.Num());
	treeIds.SetNum(meshComponents.Num());
	for (const TreeInstance& tree : trees) {
		nextTreeId = FMath::Max(nextTreeId, tree.id + 1);
		if (tree.meshIndex < 0 || tree.meshIndex >= localMeshIndex.Num()) continue;
		int32 meshIndex = localMeshIndex[tree.meshIndex];
		if (meshIndex == INDEX_NONE) continue;
		transforms[meshIndex].Add(MakeTreeTransform(tree.x, tree.y, tree.yaw, tree.scale));
		treeIds[meshIndex].Add(tree.id);
	}

	int32 total = 0;
	for (int32 meshIndex = 0; meshIndex < meshComponents.Num(); meshIndex++) {
		if (transforms[meshIndex].Num() == 0) continue;
		TArray<int32> indices = meshComponents[meshIndex]->AddInstances(transforms[meshIndex], true, true, false);
		for (int32 k = 0; k < indices.Num() && k < treeIds[meshIndex].Num(); k++) {
			RegisterInstance(treeIds[meshIndex][k], meshIndex, indices[k]);
		}
		total += indices.Num();
	}
	UE_LOG(LogTemp, Log, TEXT("UForeverVegetationFrameworkComponent: %d meshes, %d trees."), meshComponents.Num(), total);
}

int32 UForeverVegetationFrameworkComponent::AddTree(const FString& meshPath, float mapX, float mapY, float yaw, float scale) {
	int32 meshIndex;
	if (const int32* found = meshIndexByPath.Find(meshPath)) {
		meshIndex = *found;
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("UForeverVegetationFrameworkComponent: mesh %s was not preloaded, loading at runtime."), *meshPath);
		meshIndex = GetOrCreateMesh(meshPath);
	}
	if (meshIndex == INDEX_NONE) return -1;

	int32 instanceIndex = meshComponents[meshIndex]->AddInstance(MakeTreeTransform(mapX, mapY, yaw, scale), true);
	if (instanceIndex == INDEX_NONE) return -1;
	int32 treeId = nextTreeId++;
	RegisterInstance(treeId, meshIndex, instanceIndex);
	return treeId;
}

bool UForeverVegetationFrameworkComponent::RemoveTree(int32 treeId) {
	FTreeSlot slot;
	if (!treeSlots.RemoveAndCopyValue(treeId, slot)) return false;

	UHierarchicalInstancedStaticMeshComponent* hism = meshComponents[slot.meshIndex];
	TArray<int32>& ids = instanceTreeIds[slot.meshIndex];
	if (!hism->RemoveInstance(slot.instanceIndex)) return false;

	// SetRemoveSwap()之后删除是RemoveAtSwap：最后一个实例被挪到被删的位置，修正它的映射。
	int32 lastIndex = ids.Num() - 1;
	if (slot.instanceIndex != lastIndex) {
		int32 movedTreeId = ids[lastIndex];
		ids[slot.instanceIndex] = movedTreeId;
		if (FTreeSlot* moved = treeSlots.Find(movedTreeId)) moved->instanceIndex = slot.instanceIndex;
	}
	ids.RemoveAt(lastIndex);
	return true;
}

int32 UForeverVegetationFrameworkComponent::GetOrCreateMesh(const FString& meshPath) {
	if (const int32* found = meshIndexByPath.Find(meshPath)) return *found;

	AActor* owner = GetOwner();
	UStaticMesh* mesh = LoadObject<UStaticMesh>(nullptr, *meshPath);
	if (!owner || !mesh) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverVegetationFrameworkComponent: failed to load tree mesh %s."), *meshPath);
		meshIndexByPath.Add(meshPath, INDEX_NONE); // 记下失败，不重复尝试
		return INDEX_NONE;
	}

	UHierarchicalInstancedStaticMeshComponent* hism =
		NewObject<UHierarchicalInstancedStaticMeshComponent>(owner, NAME_None, RF_Transient);
	hism->SetStaticMesh(mesh);
	hism->SetMobility(EComponentMobility::Static);
	hism->SetRemoveSwap();
	hism->SetCullDistances(cullDistanceStart, cullDistanceEnd);
	hism->SetupAttachment(owner->GetRootComponent());
	hism->RegisterComponent();
	owner->AddInstanceComponent(hism);

	int32 meshIndex = meshComponents.Add(hism);
	instanceTreeIds.AddDefaulted();
	meshIndexByPath.Add(meshPath, meshIndex);
	return meshIndex;
}

FTransform UForeverVegetationFrameworkComponent::MakeTreeTransform(float mapX, float mapY, float yaw, float scale) const {
	float z = map ? map->SampleHeight(mapX, mapY) : 0.f;
	FVector location(mapX * VEGETATION_WORLD_SCALE, mapY * VEGETATION_WORLD_SCALE, z * VEGETATION_WORLD_SCALE);
	return FTransform(FRotator(0.f, FMath::RadiansToDegrees(yaw), 0.f), location, FVector(scale));
}

void UForeverVegetationFrameworkComponent::RegisterInstance(int32 treeId, int32 meshIndex, int32 instanceIndex) {
	TArray<int32>& ids = instanceTreeIds[meshIndex];
	if (ids.Num() <= instanceIndex) ids.SetNum(instanceIndex + 1);
	ids[instanceIndex] = treeId;
	treeSlots.Add(treeId, { meshIndex, instanceIndex });
}
