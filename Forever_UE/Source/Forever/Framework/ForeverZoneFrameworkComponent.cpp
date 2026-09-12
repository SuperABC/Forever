#include "Framework/ForeverZoneFrameworkComponent.h"

#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include "map/map.h"
#include "map/zone.h"
#include "map/geometry.h"

#define ZONE_WORLD_SCALE 1000.f
// 和ForeverRoadnetFrameworkComponent.cpp的ROADNET_HEIGHT_EPSILON同一类问题：Zone没有真实
// 高度数据，扁cube统一铺在Z=0，用这个偏移量避开和地形网格共面z-fighting。
#define ZONE_HEIGHT_EPSILON 10.f
// 真正做到"扁"——之前300(3m)相对典型十几米见方的footprint显得太高，压低到60(0.6m)。
#define ZONE_BOX_HEIGHT 60.f

using namespace std;

namespace {
	void ZoneAppendQuadDoubleSided(TArray<FVector>& vertices, TArray<int32>& triangles,
		const FVector& v00, const FVector& v10, const FVector& v11, const FVector& v01) {
		int32 base = vertices.Num();
		vertices.Add(v00); vertices.Add(v10); vertices.Add(v11); vertices.Add(v01);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 1);
		triangles.Add(base); triangles.Add(base + 3); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 1); triangles.Add(base + 2);
		triangles.Add(base); triangles.Add(base + 2); triangles.Add(base + 3);
	}

	// 按Quad的中心/尺寸(地图单位)+单独传入的rotation(弧度)画一个扁cube。rotation不取自Quad
	// 本身(Quad没有旋转字段)，由调用方从Zone::GetRotation()传入——和Lot::GetPosition的旋转
	// 约定完全一致(局部坐标相对中心的偏移先旋转、再平移)，这样落在斜向道路旁边的Zone/Building
	// 才能正确跟着Lot转，不会退化成轴对齐。
	void ZoneAppendFlatBox(TArray<FVector>& vertices, TArray<int32>& triangles, const Quad& quad, float rotation) {
		float cx = quad.GetPosX() * ZONE_WORLD_SCALE;
		float cy = quad.GetPosY() * ZONE_WORLD_SCALE;
		float hx = quad.GetSizeX() * 0.5f * ZONE_WORLD_SCALE;
		float hy = quad.GetSizeY() * 0.5f * ZONE_WORLD_SCALE;
		float zBottom = ZONE_HEIGHT_EPSILON;
		float zTop = ZONE_HEIGHT_EPSILON + ZONE_BOX_HEIGHT;
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

		ZoneAppendQuadDoubleSided(vertices, triangles, v001, v101, v111, v011); // 顶
		ZoneAppendQuadDoubleSided(vertices, triangles, v010, v110, v100, v000); // 底
		ZoneAppendQuadDoubleSided(vertices, triangles, v000, v100, v101, v001);
		ZoneAppendQuadDoubleSided(vertices, triangles, v100, v110, v111, v101);
		ZoneAppendQuadDoubleSided(vertices, triangles, v110, v010, v011, v111);
		ZoneAppendQuadDoubleSided(vertices, triangles, v010, v000, v001, v011);
	}
}

UForeverZoneFrameworkComponent::UForeverZoneFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> zoneFinder(
		TEXT("/Game/Asset/Materials/DefaultRoadnet.DefaultRoadnet"));
	if (zoneFinder.Succeeded()) {
		zoneBaseMaterial = zoneFinder.Object;
	}
}

void UForeverZoneFrameworkComponent::GenerateZones(Map* inMap) {
	map = inMap;
	if (!map) return;

	AActor* owner = GetOwner();
	if (!owner) return;

	if (zoneBaseMaterial) {
		zoneMaterial = UMaterialInstanceDynamic::Create(zoneBaseMaterial, this);
	}

	zoneMesh = NewObject<UProceduralMeshComponent>(owner, TEXT("Zones"));
	zoneMesh->SetupAttachment(owner->GetRootComponent());
	zoneMesh->RegisterComponent();
	owner->AddInstanceComponent(zoneMesh);

	TArray<FVector> vertices;
	TArray<int32> triangles;
	for (Zone* zone : map->GetZones()) {
		if (!zone) continue;
		ZoneAppendFlatBox(vertices, triangles, *zone, zone->GetRotation());
	}

	if (triangles.Num() > 0) {
		zoneMesh->CreateMeshSection(0, vertices, triangles,
			TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
		if (zoneMaterial) zoneMesh->SetMaterial(0, zoneMaterial);
	}
}
