#include "Framework/ForeverGreenFrameworkComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#include "map/map.h"
#include "map/green.h"

// 地图单位(10m) -> UE单位(cm)，和Zone/Building一致。
#define GREEN_WORLD_SCALE 1000.f

UForeverGreenFrameworkComponent::UForeverGreenFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = false;
}

UInstancedStaticMeshComponent* UForeverGreenFrameworkComponent::GetOrCreateISM(const FString& meshPath) {
	if (TObjectPtr<UInstancedStaticMeshComponent>* found = meshInstances.Find(meshPath)) return *found;
	AActor* owner = GetOwner();
	if (!owner) return nullptr;
	UStaticMesh* mesh = LoadObject<UStaticMesh>(nullptr, *meshPath);
	if (!mesh) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverGreenFrameworkComponent: failed to load mesh %s"), *meshPath);
		meshInstances.Add(meshPath, nullptr); // 记下失败，同一路径不再重试刷屏
		return nullptr;
	}
	UInstancedStaticMeshComponent* ism = NewObject<UInstancedStaticMeshComponent>(owner, NAME_None, RF_Transient);
	ism->SetStaticMesh(mesh);
	ism->SetupAttachment(owner->GetRootComponent());
	ism->SetMobility(EComponentMobility::Static); // Mobility要在RegisterComponent前设好，同Zone/Building
	ism->SetGenerateOverlapEvents(false);
	ism->RegisterComponent();
	owner->AddInstanceComponent(ism);
	meshInstances.Add(meshPath, ism);
	return ism;
}

void UForeverGreenFrameworkComponent::GenerateGreens(Map* inMap) {
	if (!inMap) return;
	for (const Green* green : inMap->GetGreens()) {
		if (!green) continue;
		for (const DecorationInstance& decoration : green->GetDecorations()) {
			UInstancedStaticMeshComponent* ism = GetOrCreateISM(UTF8_TO_TCHAR(decoration.meshPath.c_str()));
			if (!ism) continue;
			// Core给的z是离地高度(地图单位)，UE层加上该处地形高度；缩放是资产原尺寸的倍数。
			const float ground = inMap->SampleHeight(decoration.x, decoration.y);
			FTransform transform(FRotator(decoration.pitch, FMath::RadiansToDegrees(decoration.yaw), decoration.roll),
				FVector(decoration.x * GREEN_WORLD_SCALE, decoration.y * GREEN_WORLD_SCALE, (ground + decoration.z) * GREEN_WORLD_SCALE),
				FVector(decoration.scaleX, decoration.scaleY, decoration.scaleZ));
			ism->AddInstance(transform);
		}
	}
}
