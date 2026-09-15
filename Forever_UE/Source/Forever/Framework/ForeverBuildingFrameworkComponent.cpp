#include "Framework/ForeverBuildingFrameworkComponent.h"

#include "Element/BuildingElement.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include "map/map.h"
#include "map/building.h"

using namespace std;

UForeverBuildingFrameworkComponent::UForeverBuildingFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> buildingFinder(
		TEXT("/Game/Asset/Materials/Pure.Pure"));
	if (buildingFinder.Succeeded()) {
		buildingBaseMaterial = buildingFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> cubeFinder(
		TEXT("/Game/Asset/Meshes/Cube.Cube"));
	if (cubeFinder.Succeeded()) {
		cubeMesh = cubeFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> stairFinder(
		TEXT("/Game/Asset/Meshes/Stair.Stair"));
	if (stairFinder.Succeeded()) {
		defaultStairMesh = stairFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> rampFinder(
		TEXT("/Game/Asset/Meshes/Ramp.Ramp"));
	if (rampFinder.Succeeded()) {
		defaultRampMesh = rampFinder.Object;
	}
	// 窗户资产有问题，用户明确要求直接删掉窗户显示逻辑(见ABuildingElement::
	// BuildWallsForElement)——不再加载Window.Window，窗户现在只是墙上的一个几何缺口，和门
	// 一样不生成任何东西。

	static ConstructorHelpers::FObjectFinder<UStaticMesh> cabinFinder(
		TEXT("/Game/Asset/Meshes/Elevator.Elevator"));
	if (cabinFinder.Succeeded()) {
		defaultCabinMesh = cabinFinder.Object;
	}
}

void UForeverBuildingFrameworkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	map = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UForeverBuildingFrameworkComponent::GenerateBuildings(Map* inMap) {
	map = inMap;
	if (!map) return;
	if (!GetOwner()) return;

	if (buildingBaseMaterial) {
		defaultLodMaterial = UMaterialInstanceDynamic::Create(buildingBaseMaterial, this);
		if (defaultLodMaterial) {
			defaultLodMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.5f, 0.5f, 0.5f));
		}
		defaultWallMaterial = UMaterialInstanceDynamic::Create(buildingBaseMaterial, this);
		defaultFloorMaterial = UMaterialInstanceDynamic::Create(buildingBaseMaterial, this);
		defaultCeilingMaterial = UMaterialInstanceDynamic::Create(buildingBaseMaterial, this);
		// 墙/地/顶默认都是白色(Pure材质本身默认色)，这次不分内外墙，只有一份默认墙体材质。
	}

	// 独立建筑和园区内部建筑一视同仁，都在这里统一渲染(不再跳过GetParentZone()不为空的)。
	// 每栋building各SpawnActor一个专属的ABuildingElement——LOD状态机/近远处组件/电梯轿厢
	// 全部由Element自己管理，这里只负责创建+绑定，见ForeverBuildingFrameworkComponent.h
	// 顶部注释。
	for (auto& [name, building] : map->GetBuildings()) {
		if (!building) continue;

		ABuildingElement* element = GetWorld()->SpawnActor<ABuildingElement>();
		if (!element) continue;
		element->Init(building, this);
	}
}

UMaterialInstanceDynamic* UForeverBuildingFrameworkComponent::ResolveLodMaterial(Building* building) {
	return ResolveMaterial(building->GetLodMaterialPath(), defaultLodMaterial);
}

UMaterialInstanceDynamic* UForeverBuildingFrameworkComponent::ResolveMaterial(const string& softPath,
	UMaterialInstanceDynamic* fallback) {
	if (softPath.empty()) return fallback;

	FString key = UTF8_TO_TCHAR(softPath.c_str());
	if (auto* found = lodMaterialCache.Find(key)) {
		return *found;
	}

	UMaterialInterface* base = LoadObject<UMaterialInterface>(nullptr, *key);
	if (!base) return fallback;

	UMaterialInstanceDynamic* mid = UMaterialInstanceDynamic::Create(base, this);
	lodMaterialCache.Add(key, mid);
	return mid;
}

UStaticMesh* UForeverBuildingFrameworkComponent::ResolveMesh(const string& softPath, UStaticMesh* fallback) {
	if (softPath.empty()) return fallback;

	FString key = UTF8_TO_TCHAR(softPath.c_str());
	if (auto* found = meshCache.Find(key)) {
		return *found;
	}

	UStaticMesh* mesh = LoadObject<UStaticMesh>(nullptr, *key);
	if (!mesh) return fallback;

	meshCache.Add(key, mesh);
	return mesh;
}

void UForeverBuildingFrameworkComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 每帧重置一次全地图共享的LOD操作预算，供所有ABuildingElement在自己的Tick里申请
	// (TryConsumeLodOpBudget)——具体队列/执行现在都在Element自己身上，这里只保留节流总闸。
	frameOpBudgetRemaining = maxLodOpsPerTick;
}
