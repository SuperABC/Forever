#include "Framework/ForeverBuildingFrameworkComponent.h"

#include "Element/BuildingElement.h"
#include "Framework/ForeverFrameworkActor.h"

#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#if WITH_EDITOR
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
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
		// 有楼层的建筑会建外壳，Element建完调NotifyShellBuilt；全部到齐后一次性提交合并的ISM。
		if (building->GetBasementCount() + building->GetLayerCount() > 0) RegisterShellBuilding();
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
	frameShellBudgetRemaining = maxShellOpsPerTick;

	// 外壳阴影：每0.25秒按玩家位置刷一次(只有离玩家近的单元投射阴影)。
	shellShadowTimer += DeltaTime;
	if (bShellFlushed && shellShadowTimer >= 0.25f) {
		shellShadowTimer = 0.f;
		UpdateShellShadows();
	}

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

int32 UForeverBuildingFrameworkComponent::ShellCellOf(float mapCoord) const {
	return shellCellSize > 0.f ? FMath::FloorToInt(mapCoord / shellCellSize) : 0;
}

void UForeverBuildingFrameworkComponent::ShellCellRange(int32 cx, int32 cy, FVector2D& outMin, FVector2D& outMax) const {
	if (shellCellSize <= 0.f) {
		outMin = FVector2D(-1e9, -1e9);
		outMax = FVector2D(1e9, 1e9);
		return;
	}
	outMin = FVector2D(cx * shellCellSize, cy * shellCellSize);
	outMax = outMin + FVector2D(shellCellSize, shellCellSize);
}

UInstancedStaticMeshComponent* UForeverBuildingFrameworkComponent::CreateShellISM(UStaticMesh* mesh) {
	AActor* owner = GetOwner();
	if (!owner || !mesh) return nullptr;
	UInstancedStaticMeshComponent* ism = NewObject<UInstancedStaticMeshComponent>(owner, NAME_None, RF_Transient);
	ism->SetStaticMesh(mesh);
	ism->SetupAttachment(owner->GetRootComponent());
	// 常驻：开局提交一次实例，之后不再增删。
	ism->SetMobility(EComponentMobility::Static);
	ism->SetCollisionProfileName(TEXT("BlockAll"));
	ism->SetGenerateOverlapEvents(false);
	// 每实例自定义数据：[0]=cos [1]=sin，建筑朝向的水平单位向量，材质里用PerInstanceCustomData读。
	ism->NumCustomDataFloats = 2;
	ism->SetCastShadow(false); // 提交后由UpdateShellShadows按距离开
	return ism;
}

void UForeverBuildingFrameworkComponent::QueueShellCube(const FVector2D& cellPos, const FTransform& transform,
	float cosR, float sinR, UMaterialInterface* material) {
	FShellCubeKey key;
	key.cx = ShellCellOf(cellPos.X);
	key.cy = ShellCellOf(cellPos.Y);
	key.material = material;
	FShellPool& pool = shellCubePools.FindOrAdd(key);
	if (pool.pending.IsEmpty() && !pool.ism) ShellCellRange(key.cx, key.cy, pool.cellMin, pool.cellMax);
	pool.pending.Add(transform);
	pool.pendingCustomData.Add(cosR);
	pool.pendingCustomData.Add(sinR);
}

void UForeverBuildingFrameworkComponent::QueueShellWall(const FVector2D& cellPos, const FTransform& transform,
	float cosR, float sinR, const FWallMaterialKey& wall) {
	FShellWallKey key;
	key.cx = ShellCellOf(cellPos.X);
	key.cy = ShellCellOf(cellPos.Y);
	key.wall = wall;
	FShellPool& pool = shellWallPools.FindOrAdd(key);
	if (pool.pending.IsEmpty() && !pool.ism) ShellCellRange(key.cx, key.cy, pool.cellMin, pool.cellMax);
	pool.pending.Add(transform);
	pool.pendingCustomData.Add(cosR);
	pool.pendingCustomData.Add(sinR);
}

void UForeverBuildingFrameworkComponent::NotifyShellBuilt() {
	builtShellBuildings++;
	if (!bShellFlushed && builtShellBuildings >= expectedShellBuildings) FlushShellInstances();
}

void UForeverBuildingFrameworkComponent::FlushShellInstances() {
	bShellFlushed = true;
	AActor* owner = GetOwner();
	if (!owner) return;
	const double startTime = FPlatformTime::Seconds();
	int32 poolCount = 0;
	int64 instanceCount = 0;

	auto commit = [&](FShellPool& pool, UStaticMesh* mesh, auto&& applyMaterials) {
		if (pool.pending.IsEmpty()) return;
		UInstancedStaticMeshComponent* ism = CreateShellISM(mesh);
		if (!ism) { pool.pending.Empty(); pool.pendingCustomData.Empty(); return; }
		applyMaterials(ism);
		// 整组件距离剔除(引擎在渲染时判断，不重建代理)：视点离该单元包围球超过这个距离就不画，含阴影；碰撞不受影响。
		if (shellCullDistance > 0.f) ism->SetCullDistance(shellCullDistance * 1000.f);
		ism->RegisterComponent();
		owner->AddInstanceComponent(ism);
		ism->AddInstances(pool.pending, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);
		// 整批写自定义数据(逐实例SetCustomDataValue在几十万实例上太慢)；AddInstances已把数组扩成Num*2个0。
		if (ism->PerInstanceSMCustomData.Num() == pool.pendingCustomData.Num()) {
			ism->PerInstanceSMCustomData = MoveTemp(pool.pendingCustomData);
		}
		ism->MarkRenderStateDirty();
		pool.ism = ism;
		poolCount++;
		instanceCount += pool.pending.Num();
		pool.pending.Empty();
		pool.pendingCustomData.Empty();
	};

	for (auto& [key, pool] : shellCubePools) {
		commit(pool, cubeMesh, [&](UInstancedStaticMeshComponent* ism) { if (key.material) ism->SetMaterial(0, key.material); });
	}
	UStaticMesh* wallMesh = shellWallPools.IsEmpty() ? nullptr : ResolveMesh("/Game/Asset/Building/Mesh/Wall.Wall", nullptr);
	for (auto& [key, pool] : shellWallPools) {
		commit(pool, wallMesh, [&](UInstancedStaticMeshComponent* ism) {
			for (int32 i = 0; i < 5; i++) if (key.wall.slots[i]) ism->SetMaterial(i, key.wall.slots[i]);
			});
	}
	if (shellCullDistance > 0.f && cubeMesh && cubeMesh->GetRenderData() && cubeMesh->GetRenderData()->HasValidNaniteData()) {
		UE_LOG(LogTemp, Warning, TEXT("Building shell: Cube mesh still has Nanite enabled, shellCullDistance will not take effect."));
	}
	UE_LOG(LogTemp, Log, TEXT("Building shell merged (cell size %.0f): %d ISM components (%d cube pools, %d wall pools), %lld instances, %.0f ms"),
		shellCellSize, poolCount, shellCubePools.Num(), shellWallPools.Num(), instanceCount,(FPlatformTime::Seconds() - startTime) * 1000.0);
	UpdateShellShadows();
}

void UForeverBuildingFrameworkComponent::UpdateShellShadows() {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn) return;
	const FVector location = pawn->GetActorLocation() / 1000.f; // UE单位->地图单位
	const FVector2D player(location.X, location.Y);
	// 玩家到单元矩形的水平距离不超过shellShadowDistance就投射阴影。
	const float radius = shellShadowDistance;
	auto update = [&](FShellPool& pool) {
		if (!pool.ism) return;
		const float dx = FMath::Max(FMath::Max(pool.cellMin.X - player.X, 0.0), player.X - pool.cellMax.X);
		const float dy = FMath::Max(FMath::Max(pool.cellMin.Y - player.Y, 0.0), player.Y - pool.cellMax.Y);
		const float distSq = dx * dx + dy * dy;
		const bool want = shellCellSize <= 0.f || distSq <= radius * radius;
		if (want != pool.casting) {
			pool.casting = want;
			pool.ism->SetCastShadow(want);
		}

	};
	for (auto& [key, pool] : shellCubePools) update(pool);
	for (auto& [key, pool] : shellWallPools) update(pool);
}
