#include "Framework/ForeverBuildingFrameworkComponent.h"

#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"

#include "map/map.h"
#include "map/building.h"
#include "map/geometry.h"

#define BUILDING_WORLD_SCALE 1000.f
// 和ForeverZoneFrameworkComponent.cpp的ZONE_HEIGHT_EPSILON同一类问题：Building没有真实
// 高度数据，楼层cube统一从这个高度起算(相当于室外地坪)，避开和地形网格共面z-fighting。
#define BUILDING_HEIGHT_EPSILON 10.f

using namespace std;

namespace {
	void BuildingAppendQuadDoubleSided(TArray<FVector>& vertices, TArray<int32>& triangles,
		const FVector& v00, const FVector& v10, const FVector& v11, const FVector& v01) {
		int32 base = vertices.Num();
		vertices.Add(v00); vertices.Add(v10); vertices.Add(v11); vertices.Add(v01);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 1);
		triangles.Add(base); triangles.Add(base + 3); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 1); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 3);
	}

	// 按显式给出的中心/半尺寸(世界单位)/Z范围(世界单位)/rotation(弧度)画一个cube——原来是
	// 接一个整Quad+rotation，这次改成接显式参数，因为楼体footprint是Building自身Quad的一个
	// 子矩形，而且每层楼的Z范围各不相同，不能再直接用Quad本身的尺寸。
	void BuildingAppendFlatBoxExplicit(TArray<FVector>& vertices, TArray<int32>& triangles,
		float cx, float cy, float hx, float hy, float zBottom, float zTop, float rotation) {
		float c = FMath::Cos(rotation);
		float s = FMath::Sin(rotation);

		auto RotatedCorner = [&](float relX, float relY, float z) {
			return FVector(cx + relX * c - relY * s, cy + relX * s + relY * c, z);
			};

		FVector v000 = RotatedCorner(-hx, -hy, zBottom);
		FVector v100 = RotatedCorner(hx, -hy, zBottom);
		FVector v110 = RotatedCorner(hx, hy, zBottom);
		FVector v010 = RotatedCorner(-hx, hy, zBottom);
		FVector v001 = RotatedCorner(-hx, -hy, zTop);
		FVector v101 = RotatedCorner(hx, -hy, zTop);
		FVector v111 = RotatedCorner(hx, hy, zTop);
		FVector v011 = RotatedCorner(-hx, hy, zTop);

		BuildingAppendQuadDoubleSided(vertices, triangles, v001, v101, v111, v011); // 顶
		BuildingAppendQuadDoubleSided(vertices, triangles, v010, v110, v100, v000); // 底
		BuildingAppendQuadDoubleSided(vertices, triangles, v000, v100, v101, v001);
		BuildingAppendQuadDoubleSided(vertices, triangles, v100, v110, v111, v101);
		BuildingAppendQuadDoubleSided(vertices, triangles, v110, v010, v011, v111);
		BuildingAppendQuadDoubleSided(vertices, triangles, v010, v000, v001, v011);
	}

	// 楼体子矩形的世界中心(地图单位换算成世界单位)——building自身的GetPosX/PosY()是Building
	// 整个Quad的中心(已经是世界坐标)，楼体中心相对这个点有一个未旋转局部坐标系下的偏移
	// (GetBodyOffsetX/Y())，这个偏移要跟着building自己的rotation一起转，才能得到楼体真正的
	// 世界中心——和Building本身"局部坐标先旋转再平移"的约定一致。
	void ComputeBodyWorldCenter(const Building& building, float& outCx, float& outCy) {
		float cx = building.GetPosX() * BUILDING_WORLD_SCALE;
		float cy = building.GetPosY() * BUILDING_WORLD_SCALE;
		float rot = building.GetRotation();
		float c = FMath::Cos(rot), s = FMath::Sin(rot);
		float offX = building.GetBodyOffsetX() * BUILDING_WORLD_SCALE;
		float offY = building.GetBodyOffsetY() * BUILDING_WORLD_SCALE;
		outCx = cx + offX * c - offY * s;
		outCy = cy + offX * s + offY * c;
	}

	// 楼层i(0-based，横跨basements+layers整个floorHeights数组)的Z范围(世界单位)——
	// basements部分(i<basements)往BUILDING_HEIGHT_EPSILON(地坪)以下堆叠，地上部分从地坪往上
	// 堆叠。floorHeights[basements-1]是最接近地坪的地下室(如果有)，floorHeights[basements]
	// 是1楼。
	void ComputeFloorZRange(const Building& building, int32 floorIndex, float& outZBottom, float& outZTop) {
		const vector<float>& heights = building.GetFloorHeights();
		int32 basements = building.GetBasementCount();
		float grade = BUILDING_HEIGHT_EPSILON;

		if (floorIndex < basements) {
			float depthToTop = 0.f;
			for (int32 i = floorIndex + 1; i < basements; i++) depthToTop += heights[i];
			float depthToBottom = depthToTop + heights[floorIndex];
			outZTop = grade - depthToTop * BUILDING_WORLD_SCALE;
			outZBottom = grade - depthToBottom * BUILDING_WORLD_SCALE;
		}
		else {
			float heightToBottom = 0.f;
			for (int32 i = basements; i < floorIndex; i++) heightToBottom += heights[i];
			float heightToTop = heightToBottom + heights[floorIndex];
			outZBottom = grade + heightToBottom * BUILDING_WORLD_SCALE;
			outZTop = grade + heightToTop * BUILDING_WORLD_SCALE;
		}
	}

	// 整栋建筑的Z范围(世界单位)——最深地下室的底到最高楼层的顶，供远处LOD的单体cube使用。
	void ComputeFullZRange(const Building& building, float& outZBottom, float& outZTop) {
		const vector<float>& heights = building.GetFloorHeights();
		int32 basements = building.GetBasementCount();
		float totalBasementDepth = 0.f;
		for (int32 i = 0; i < basements; i++) totalBasementDepth += heights[i];
		float totalAboveHeight = 0.f;
		for (int32 i = basements; i < static_cast<int32>(heights.size()); i++) totalAboveHeight += heights[i];
		float grade = BUILDING_HEIGHT_EPSILON;
		outZBottom = grade - totalBasementDepth * BUILDING_WORLD_SCALE;
		outZTop = grade + totalAboveHeight * BUILDING_WORLD_SCALE;
	}
}

UForeverBuildingFrameworkComponent::UForeverBuildingFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> buildingFinder(
		TEXT("/Game/Asset/Materials/Pure.Pure"));
	if (buildingFinder.Succeeded()) {
		buildingBaseMaterial = buildingFinder.Object;
	}
}

void UForeverBuildingFrameworkComponent::GenerateBuildings(Map* inMap) {
	map = inMap;
	if (!map) return;

	AActor* owner = GetOwner();
	if (!owner) return;

	if (buildingBaseMaterial) {
		buildingMaterial = UMaterialInstanceDynamic::Create(buildingBaseMaterial, this);
		defaultLodMaterial = UMaterialInstanceDynamic::Create(buildingBaseMaterial, this);
		if (defaultLodMaterial) {
			defaultLodMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.5f, 0.5f, 0.5f));
		}
	}

	buildingMesh = NewObject<UProceduralMeshComponent>(owner, TEXT("Buildings"));
	buildingMesh->SetupAttachment(owner->GetRootComponent());
	buildingMesh->RegisterComponent();
	owner->AddInstanceComponent(buildingMesh);

	buildingLodMesh = NewObject<UProceduralMeshComponent>(owner, TEXT("BuildingsLod"));
	buildingLodMesh->SetupAttachment(owner->GetRootComponent());
	buildingLodMesh->RegisterComponent();
	owner->AddInstanceComponent(buildingLodMesh);

	// 独立建筑和园区内部建筑一视同仁，都在这里统一渲染(不再跳过GetParentZone()不为空的)。
	for (auto& [name, building] : map->GetBuildings()) {
		if (!building) continue;

		FBuildingRenderState state;
		state.nearFloorCount = building->GetBasementCount() + building->GetLayerCount();
		state.nearSectionBase = nextNearSectionIndex;
		nextNearSectionIndex += state.nearFloorCount;
		state.farSectionIndex = nextFarSectionIndex++;
		state.currentLod = EBuildingLod::Far;

		BuildFarSection(building, state);
		renderStates.Add(building, state);
	}
}

UMaterialInstanceDynamic* UForeverBuildingFrameworkComponent::ResolveLodMaterial(Building* building) {
	const string& path = building->GetLodMaterialPath();
	if (path.empty()) return defaultLodMaterial;

	FString key = UTF8_TO_TCHAR(path.c_str());
	if (auto* found = lodMaterialCache.Find(key)) {
		return *found;
	}

	UMaterialInterface* base = LoadObject<UMaterialInterface>(nullptr, *key);
	if (!base) return defaultLodMaterial;

	UMaterialInstanceDynamic* mid = UMaterialInstanceDynamic::Create(base, this);
	lodMaterialCache.Add(key, mid);
	return mid;
}

void UForeverBuildingFrameworkComponent::BuildFarSection(Building* building, FBuildingRenderState& state) {
	if (!buildingLodMesh) return;

	float cx, cy;
	ComputeBodyWorldCenter(*building, cx, cy);
	float hx = building->GetBodySizeX() * 0.5f * BUILDING_WORLD_SCALE;
	float hy = building->GetBodySizeY() * 0.5f * BUILDING_WORLD_SCALE;
	float zBottom, zTop;
	ComputeFullZRange(*building, zBottom, zTop);

	TArray<FVector> vertices;
	TArray<int32> triangles;
	BuildingAppendFlatBoxExplicit(vertices, triangles, cx, cy, hx, hy, zBottom, zTop, building->GetRotation());

	buildingLodMesh->CreateMeshSection(state.farSectionIndex, vertices, triangles,
		TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
	UMaterialInstanceDynamic* material = ResolveLodMaterial(building);
	if (material) buildingLodMesh->SetMaterial(state.farSectionIndex, material);
}

void UForeverBuildingFrameworkComponent::BuildFloorSection(Building* building, FBuildingRenderState& state, int32 floorIndex) {
	if (!buildingMesh) return;
	if (floorIndex < 0 || floorIndex >= state.nearFloorCount) return;

	float cx, cy;
	ComputeBodyWorldCenter(*building, cx, cy);
	float hx = building->GetBodySizeX() * 0.5f * BUILDING_WORLD_SCALE;
	float hy = building->GetBodySizeY() * 0.5f * BUILDING_WORLD_SCALE;
	float zBottom, zTop;
	ComputeFloorZRange(*building, floorIndex, zBottom, zTop);

	TArray<FVector> vertices;
	TArray<int32> triangles;
	BuildingAppendFlatBoxExplicit(vertices, triangles, cx, cy, hx, hy, zBottom, zTop, building->GetRotation());

	int32 section = state.nearSectionBase + floorIndex;
	buildingMesh->CreateMeshSection(section, vertices, triangles,
		TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
	if (buildingMaterial) buildingMesh->SetMaterial(section, buildingMaterial);
}

void UForeverBuildingFrameworkComponent::ClearNearSections(Building* building, FBuildingRenderState& state) {
	if (!buildingMesh) return;
	for (int32 i = 0; i < state.nearFloorCount; i++) {
		buildingMesh->ClearMeshSection(state.nearSectionBase + i);
	}
}

void UForeverBuildingFrameworkComponent::ClearFarSection(Building* building, FBuildingRenderState& state) {
	if (!buildingLodMesh) return;
	buildingLodMesh->ClearMeshSection(state.farSectionIndex);
}

void UForeverBuildingFrameworkComponent::ExecuteLodOp(const FBuildingLodOp& op) {
	FBuildingRenderState* state = renderStates.Find(op.building);
	if (!state) return;

	switch (op.type) {
	case EBuildingLodOpType::BuildFarMesh:
		BuildFarSection(op.building, *state);
		break;
	case EBuildingLodOpType::DeleteAllNearMeshes:
		ClearNearSections(op.building, *state);
		state->currentLod = EBuildingLod::Far;
		state->transitionPending = false;
		break;
	case EBuildingLodOpType::BuildFloorMesh:
		BuildFloorSection(op.building, *state, op.floorIndex);
		break;
	case EBuildingLodOpType::DeleteFarMesh:
		ClearFarSection(op.building, *state);
		state->currentLod = EBuildingLod::Near;
		state->transitionPending = false;
		break;
	}
}

void UForeverBuildingFrameworkComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!map) return;

	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn) return;
	FVector pawnMapPos = pawn->GetActorLocation() / BUILDING_WORLD_SCALE;
	FVector2D viewerXY(pawnMapPos.X, pawnMapPos.Y);

	for (auto& pair : renderStates) {
		Building* building = pair.Key;
		FBuildingRenderState& state = pair.Value;
		if (state.transitionPending || !building) continue;

		float dist = FVector2D::Distance(viewerXY, FVector2D(building->GetPosX(), building->GetPosY()));
		bool wantNear = dist <= lodSwitchDistance;

		if (wantNear && state.currentLod == EBuildingLod::Far) {
			for (int32 i = 0; i < state.nearFloorCount; i++) {
				lodOpQueue.Enqueue({ EBuildingLodOpType::BuildFloorMesh, building, i });
			}
			lodOpQueue.Enqueue({ EBuildingLodOpType::DeleteFarMesh, building, -1 });
			state.transitionPending = true;
		}
		else if (!wantNear && state.currentLod == EBuildingLod::Near) {
			lodOpQueue.Enqueue({ EBuildingLodOpType::BuildFarMesh, building, -1 });
			lodOpQueue.Enqueue({ EBuildingLodOpType::DeleteAllNearMeshes, building, -1 });
			state.transitionPending = true;
		}
	}

	int32 processed = 0;
	FBuildingLodOp op;
	while (processed < maxLodOpsPerTick && lodOpQueue.Dequeue(op)) {
		ExecuteLodOp(op);
		processed++;
	}
}
