#include "Framework/ForeverBuildingFrameworkComponent.h"

#include "Element/BuildingElement.h"
#include "Framework/ForeverFrameworkActor.h"

#include "Engine/StaticMesh.h"
#if WITH_EDITOR
#include "StaticMeshCompiler.h"
#endif
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"

#include "map/map.h"
#include "map/building.h"

using namespace std;

UForeverBuildingFrameworkComponent::UForeverBuildingFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> buildingFinder(
		TEXT("/Game/Asset/Common/Material/PureColor.PureColor"));
	if (buildingFinder.Succeeded()) {
		buildingBaseMaterial = buildingFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> cubeFinder(
		TEXT("/Game/Asset/Common/Mesh/Cube.Cube"));
	if (cubeFinder.Succeeded()) {
		cubeMesh = cubeFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> stairFinder(
		TEXT("/Game/Asset/Building/Mesh/Stair_Return.Stair_Return"));
	if (stairFinder.Succeeded()) {
		defaultStairMesh = stairFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> rampFinder(
		TEXT("/Game/Asset/Building/Mesh/Ramp_2cm.Ramp_2cm"));
	if (rampFinder.Succeeded()) {
		defaultRampMesh = rampFinder.Object;
	}
	// 窗户不加载网格：它只是墙上的一个几何缺口(见ABuildingElement::BuildWallsForElement)。

	static ConstructorHelpers::FObjectFinder<UStaticMesh> cabinFinder(
		TEXT("/Game/Asset/Building/Mesh/Elevator.Elevator"));
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
		// 墙/地/顶默认都是白色(Pure材质本身默认色)，不分内外墙，只有一份默认墙体材质。
	}

	// 独立建筑和园区内部建筑一视同仁，都在这里统一创建Element；LOD/电梯轿厢全部由Element自己管理。
	for (auto& [name, building] : map->GetBuildings()) {
		if (!building) continue;
		// 0层且没有自定义模型的建筑(ParkBuilding这类只有植被的)不生成Element；0层但有自定义
		// 模型的(MoharBuilding)要生成——模型靠Element的近处LOD加载，Element自己会跳过远处方块
		// 和进出触发盒，见BuildingElement.md"自定义模型"一节。
		if (building->GetBasementCount() + building->GetLayerCount() == 0 && building->GetDecorations().empty()) continue;

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

	// 世界因为RequestFreezeUntilLodSettled被冻结、且所有building都已经排空LOD切换队列——
	// 恢复正常时间倍率。这个检查本身不受时间倍率影响（TickComponent仍然按正常帧率被调用，
	// 只是DeltaTime被缩放），所以冻结期间也能正常侦测到"排空"这一刻。
	if (IsWorldGenerationDone()) {
		if (!bMeshesPrewarmed) PrewarmMeshes();
		postGenerationFrames++;
	}
	if (bFrozenForLod && postGenerationFrames > kPostGenerationGateFrames && pendingLodTransitionCount <= 0) {
		bFrozenForLod = false;
		UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.f);
	}
}

void UForeverBuildingFrameworkComponent::PrewarmMeshes() {
	bMeshesPrewarmed = true;
	const AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(GetOwner());
	TSet<FString> paths = { TEXT("/Game/Asset/Building/Mesh/Wall.Wall") };
	if (frameworkActor && frameworkActor->GetMap()) {
		for (const auto& [name, building] : frameworkActor->GetMap()->GetBuildings()) {
			if (!building) continue;
			for (const auto& [level, spec] : building->GetFloorSpecs()) {
				if (!spec.assets.stairMeshPath.empty()) paths.Add(UTF8_TO_TCHAR(spec.assets.stairMeshPath.c_str()));
				if (!spec.assets.rampMeshPath.empty()) paths.Add(UTF8_TO_TCHAR(spec.assets.rampMeshPath.c_str()));
			}
		}
	}
	TArray<UStaticMesh*> loaded;
	for (const FString& path : paths) {
		if (UStaticMesh* mesh = ResolveMesh(string(TCHAR_TO_UTF8(*path)), nullptr)) loaded.Add(mesh);
	}
#if WITH_EDITOR
	// 静态网格异步编译只在编辑器里有；打包后的网格是烘焙好的，加载完就能用。
	if (loaded.Num() > 0) FStaticMeshCompilingManager::Get().FinishCompilation(loaded);
#endif
}

bool UForeverBuildingFrameworkComponent::IsWorldGenerationDone() const {
	const AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(GetOwner());
	return frameworkActor == nullptr || frameworkActor->IsInitialGenerationDone();
}

void UForeverBuildingFrameworkComponent::RequestFreezeUntilLodSettled() {
	bFrozenForLod = true;
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.f);
}

void UForeverBuildingFrameworkComponent::NotifyLodTransitionStarted() {
	pendingLodTransitionCount++;
}

void UForeverBuildingFrameworkComponent::NotifyLodTransitionFinished() {
	pendingLodTransitionCount = FMath::Max(0, pendingLodTransitionCount - 1);
}
