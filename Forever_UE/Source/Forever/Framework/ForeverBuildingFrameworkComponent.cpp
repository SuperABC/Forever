#include "Framework/ForeverBuildingFrameworkComponent.h"

#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include "map/map.h"
#include "map/building.h"
#include "map/geometry.h"

#define BUILDING_WORLD_SCALE 1000.f
// 和ForeverZoneFrameworkComponent.cpp的ZONE_HEIGHT_EPSILON同一类问题：Building没有真实
// 高度数据，扁cube统一铺在Z=0，用这个偏移量避开和地形网格共面z-fighting；比Zone的box高一些，
// 方便PIE里同时看到两种cube时能区分层级。
#define BUILDING_HEIGHT_EPSILON 10.f
// 真正做到"扁"——之前600(6m)相对典型footprint显得太高，压低到120(1.2m)，仍然比Zone高一档。
#define BUILDING_BOX_HEIGHT 120.f

using namespace std;

namespace {
	void AppendQuadDoubleSided(TArray<FVector>& vertices, TArray<int32>& triangles,
		const FVector& v00, const FVector& v10, const FVector& v11, const FVector& v01) {
		int32 base = vertices.Num();
		vertices.Add(v00); vertices.Add(v10); vertices.Add(v11); vertices.Add(v01);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 1);
		triangles.Add(base); triangles.Add(base + 3); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 1); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 3);
	}

	// 按Quad的中心/尺寸(地图单位)+单独传入的rotation(弧度)画一个扁cube。rotation不取自Quad
	// 本身(Quad没有旋转字段)，由调用方从Building::GetRotation()传入——和Lot::GetPosition的
	// 旋转约定完全一致(局部坐标相对中心的偏移先旋转、再平移)，这样落在斜向道路旁边的
	// Zone/Building才能正确跟着Lot转，不会退化成轴对齐。
	void AppendFlatBox(TArray<FVector>& vertices, TArray<int32>& triangles, const Quad& quad, float rotation) {
		float cx = quad.GetPosX() * BUILDING_WORLD_SCALE;
		float cy = quad.GetPosY() * BUILDING_WORLD_SCALE;
		float hx = quad.GetSizeX() * 0.5f * BUILDING_WORLD_SCALE;
		float hy = quad.GetSizeY() * 0.5f * BUILDING_WORLD_SCALE;
		float zBottom = BUILDING_HEIGHT_EPSILON;
		float zTop = BUILDING_HEIGHT_EPSILON + BUILDING_BOX_HEIGHT;
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

		AppendQuadDoubleSided(vertices, triangles, v001, v101, v111, v011); // 顶
		AppendQuadDoubleSided(vertices, triangles, v010, v110, v100, v000); // 底
		AppendQuadDoubleSided(vertices, triangles, v000, v100, v101, v001);
		AppendQuadDoubleSided(vertices, triangles, v100, v110, v111, v101);
		AppendQuadDoubleSided(vertices, triangles, v110, v010, v011, v111);
		AppendQuadDoubleSided(vertices, triangles, v010, v000, v001, v011);
	}
}

UForeverBuildingFrameworkComponent::UForeverBuildingFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> buildingFinder(
		TEXT("/Game/Asset/Materials/White.White"));
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
	}

	buildingMesh = NewObject<UProceduralMeshComponent>(owner, TEXT("Buildings"));
	buildingMesh->SetupAttachment(owner->GetRootComponent());
	buildingMesh->RegisterComponent();
	owner->AddInstanceComponent(buildingMesh);

	TArray<FVector> vertices;
	TArray<int32> triangles;
	for (Building* building : map->GetBuildings()) {
		if (!building) continue;
		AppendFlatBox(vertices, triangles, *building, building->GetRotation());
	}

	if (triangles.Num() > 0) {
		buildingMesh->CreateMeshSection(0, vertices, triangles,
			TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
		if (buildingMaterial) buildingMesh->SetMaterial(0, buildingMaterial);
	}
}
