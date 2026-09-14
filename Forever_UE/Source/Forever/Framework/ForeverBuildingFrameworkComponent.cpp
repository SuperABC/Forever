#include "Framework/ForeverBuildingFrameworkComponent.h"

#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"

#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "map/geometry.h"

#include <algorithm>
#include <array>

#define BUILDING_WORLD_SCALE 1000.f
// 和ForeverZoneFrameworkComponent.cpp的ZONE_HEIGHT_EPSILON同一类问题：Building没有真实
// 高度数据，楼层cube统一从这个高度起算(相当于室外地坪)，避开和地形网格共面z-fighting。
#define BUILDING_HEIGHT_EPSILON 10.f
// 墙体厚度(地图单位)，照抄老工程BuildingBase.cpp::ConstructQuad里写死的0.01f。
#define BUILDING_WALL_THICKNESS 0.01f
// UE标准立方体静态网格(/Game/Asset/Meshes/Cube.Cube)的原生边长(cm)——SpawnCube用
// SetWorldScale3D把它缩放到目标尺寸，缩放系数=目标尺寸/这个原生边长。
#define BUILDING_CUBE_MESH_SIZE 100.f

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

	// 按显式给出的中心/半尺寸(世界单位)/Z范围(世界单位)/rotation(弧度)画一个cube——远处LOD
	// (整栋一个box)专用，近处楼层几何这次改用独立UStaticMeshComponent(SpawnCube)，不再共用
	// 这个PMC写法。
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
	// 世界中心——和Building本身"局部坐标先旋转再平移"的约定一致。只给远处LOD(整栋一个box)用；
	// 楼层内部任意一点的转换见下面更通用的ComputeWorldPosition。
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

	// 把Floor局部坐标(原点在楼体左下角，未旋转——Core侧Stair/Corridor/Room等InstanciateQuad()
	// 换算出来的坐标系，和Building::LocalToWorld同一套约定)转换成世界坐标(UE世界单位)——和
	// ComputeBodyWorldCenter是同一个公式，只是这里支持楼体内部任意一点，不止楼体中心。
	void ComputeWorldPosition(const Building& building, float localX, float localY,
		float& outWorldX, float& outWorldY) {
		float relX = localX - building.GetBodySizeX() * 0.5f + building.GetBodyOffsetX();
		float relY = localY - building.GetBodySizeY() * 0.5f + building.GetBodyOffsetY();
		float rot = building.GetRotation();
		float c = FMath::Cos(rot), s = FMath::Sin(rot);
		outWorldX = (building.GetPosX() + relX * c - relY * s) * BUILDING_WORLD_SCALE;
		outWorldY = (building.GetPosY() + relX * s + relY * c) * BUILDING_WORLD_SCALE;
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

	// 一个开口(门/窗)在这一侧墙上的位置——x1/x2沿墙方向(局部坐标，相对这面墙起点horizBase的
	// 偏移量，不是绝对局部坐标)，y1/y2是离楼板的高度(世界单位)。门/窗对墙体开洞分段来说完全
	// 一样(都只是缺口)，这次不再摆窗户网格，不需要区分门/窗类型。
	struct FBuildingWallOpening {
		float x1 = 0.f, x2 = 0.f, y1 = 0.f, y2 = 0.f;
	};

	// Stair/Ramp网格的朝向由三部分组成：building自身的世界旋转(rotation，跟着lot走，和墙体/
	// 地板slab一致)、加上这个楼梯/坡道自己的局部direction(0-3，模板解析时已经按AssignFloor
	// 声明的face预旋转过，见BuildingLayoutLibrary)——照抄老工程ABuildingBase::ConstructBuilding
	// 里"GetRotation(stair.GetDirection())"这段：NORTH=0°、EAST=90°、SOUTH=180°、WEST=270°，
	// 在老工程里这是相对building自己Actor(已经摆好世界旋转)的局部附加旋转，这次没有per-building
	// 的Actor，要显式把这段角度和building世界旋转相加才能等价；同时WEST/EAST方向要交换
	// sizeX/sizeY(老工程同一段代码"if WEST||EAST则FVector(sizeY,sizeX,...)否则FVector(sizeX,
	// sizeY,...)")——网格局部坐标系X轴默认对齐NORTH朝向下的世界X，旋转90°/270°之后局部X/Y
	// 和世界X/Y互换，缩放必须按局部轴给，不然90°/270°朝向的楼梯会被拉伸成错误的长宽比。
	// 之前完全没做这一步，所有building的楼梯/坡道网格只贴了building自身的世界旋转，看起来
	// 永远朝同一个方向(PIE验证发现)。
	void ComputeDirectionalMeshTransform(int direction, float sizeX, float sizeY, float buildingRotation,
		float& outSizeX, float& outSizeY, float& outRotation) {
		float localAngleDeg = 0.f;
		bool swapXY = false;
		switch (direction) {
		case FACE_WEST:  localAngleDeg = 270.f; swapXY = true; break;
		case FACE_EAST:  localAngleDeg = 90.f;  swapXY = true; break;
		case FACE_SOUTH: localAngleDeg = 180.f; break;
		case FACE_NORTH: default: localAngleDeg = 0.f; break;
		}
		outSizeX = swapXY ? sizeY : sizeX;
		outSizeY = swapXY ? sizeX : sizeY;
		outRotation = buildingRotation + FMath::DegreesToRadians(localAngleDeg);
	}
}

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
	// 窗户资产有问题，用户明确要求直接删掉窗户显示逻辑(见BuildWallsForElement)——不再加载
	// Window.Window，窗户现在只是墙上的一个几何缺口，和门一样不生成任何东西。
}

void UForeverBuildingFrameworkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	// 只置空指针，不清空renderStates/lodOpQueue或去DestroyComponent那些近处LOD组件——
	// TickComponent顶部的if (!map) return;这一行本身就足够挡住后续所有对renderStates里
	// Building*的解引用，其余UObject(包括每个near-LOD的UStaticMeshComponent)跟着owner一起
	// 被引擎正常GC掉即可，不需要在这里手动收拾。
	map = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UForeverBuildingFrameworkComponent::GenerateBuildings(Map* inMap) {
	map = inMap;
	if (!map) return;

	AActor* owner = GetOwner();
	if (!owner) return;

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

	buildingLodMesh = NewObject<UProceduralMeshComponent>(owner, TEXT("BuildingsLod"));
	buildingLodMesh->SetupAttachment(owner->GetRootComponent());
	buildingLodMesh->RegisterComponent();
	owner->AddInstanceComponent(buildingLodMesh);

	// 独立建筑和园区内部建筑一视同仁，都在这里统一渲染(不再跳过GetParentZone()不为空的)。
	for (auto& [name, building] : map->GetBuildings()) {
		if (!building) continue;

		FBuildingRenderState state;
		state.nearFloorCount = building->GetBasementCount() + building->GetLayerCount();
		state.nearComponentsByFloor.SetNum(state.nearFloorCount);
		state.farSectionIndex = nextFarSectionIndex++;
		state.currentLod = EBuildingLod::Far;

		BuildFarSection(building, state);
		renderStates.Add(building, state);
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

UStaticMeshComponent* UForeverBuildingFrameworkComponent::SpawnCube(float centerX, float centerY, float centerZ,
	float sizeX, float sizeY, float sizeZ, float rotation, UMaterialInterface* material) {
	if (!cubeMesh) return nullptr;
	AActor* owner = GetOwner();
	if (!owner) return nullptr;

	UStaticMeshComponent* comp = NewObject<UStaticMeshComponent>(owner, NAME_None, RF_Transient);
	comp->SetStaticMesh(cubeMesh);
	comp->SetupAttachment(owner->GetRootComponent());
	comp->RegisterComponent();
	owner->AddInstanceComponent(comp);

	comp->SetWorldLocation(FVector(centerX, centerY, centerZ));
	comp->SetWorldRotation(FRotator(0.f, FMath::RadiansToDegrees(rotation), 0.f));
	comp->SetWorldScale3D(FVector(sizeX / BUILDING_CUBE_MESH_SIZE, sizeY / BUILDING_CUBE_MESH_SIZE,
		sizeZ / BUILDING_CUBE_MESH_SIZE));
	if (material) comp->SetMaterial(0, material);
	return comp;
}

UStaticMeshComponent* UForeverBuildingFrameworkComponent::SpawnMesh(float centerX, float centerY, float centerZ,
	float sizeX, float sizeY, float sizeZ, float rotation, UStaticMesh* mesh, UMaterialInterface* material) {
	if (!mesh) return nullptr;
	AActor* owner = GetOwner();
	if (!owner) return nullptr;

	UStaticMeshComponent* comp = NewObject<UStaticMeshComponent>(owner, NAME_None, RF_Transient);
	comp->SetStaticMesh(mesh);
	comp->SetupAttachment(owner->GetRootComponent());
	comp->RegisterComponent();
	owner->AddInstanceComponent(comp);

	comp->SetWorldLocation(FVector(centerX, centerY, centerZ));
	comp->SetWorldRotation(FRotator(0.f, FMath::RadiansToDegrees(rotation), 0.f));
	// 楼梯/坡道资产约定包围盒是边长BUILDING_CUBE_MESH_SIZE(100，和SpawnCube的单位立方体
	// Cube.Cube同一个换算基准)的正方体——同一份"资产层面统一缩放"的约定，保证以后换成
	// 不同美术资产的楼梯/坡道，只要包围盒同样是这个单位大小就能直接互相替换，不用改代码。
	// 之前这里完全没有调用SetWorldScale3D，楼梯/坡道网格一直按资产原始大小摆放，和.layout
	// 模板里Stair/Ramp实际声明的尺寸对不上(PIE验证发现)。
	comp->SetWorldScale3D(FVector(sizeX / BUILDING_CUBE_MESH_SIZE, sizeY / BUILDING_CUBE_MESH_SIZE,
		sizeZ / BUILDING_CUBE_MESH_SIZE));
	if (material) comp->SetMaterial(0, material);
	return comp;
}

void UForeverBuildingFrameworkComponent::BuildWallsForElement(Building* building, float floorBaseZ, float floorHeight,
	float elemCenterX, float elemCenterY, float elemSizeX, float elemSizeY,
	bool wallWest, bool wallEast, bool wallNorth, bool wallSouth,
	const unordered_map<int, vector<array<float, 8>>>& doors,
	const unordered_map<int, vector<array<float, 8>>>& windows,
	UMaterialInterface* wallMaterial, FBuildingRenderState& state, int32 floorIndex) {
	// floorBaseZ是Building::GetFloorBaseZ(level)的值：地图单位、相对地坪(grade)的楼层底部
	// 高度(basements为负、地上楼层为正，见building.h)，不含BUILDING_HEIGHT_EPSILON这个纯
	// 渲染层的"避免和地形共面z-fighting"偏移——这里换算世界Z时要把grade偏移加回来，和
	// ComputeFloorZRange/ComputeFullZRange同一套约定。
	float rotation = building->GetRotation();
	float floorBottomWorldZ = BUILDING_HEIGHT_EPSILON + floorBaseZ * BUILDING_WORLD_SCALE;
	float floorTopWorldZ = BUILDING_HEIGHT_EPSILON + (floorBaseZ + floorHeight) * BUILDING_WORLD_SCALE;

	float halfX = elemSizeX * 0.5f, halfY = elemSizeY * 0.5f;
	struct FFaceInfo { bool enabled; int32 dir; float horizBase; float horizSpan; bool alongY; float acrossFixed; };
	FFaceInfo faces[4] = {
		{ wallWest,  FACE_WEST,  elemCenterY - halfY, elemSizeY, true,  elemCenterX - halfX },
		{ wallEast,  FACE_EAST,  elemCenterY - halfY, elemSizeY, true,  elemCenterX + halfX },
		{ wallNorth, FACE_NORTH, elemCenterX - halfX, elemSizeX, false, elemCenterY - halfY },
		{ wallSouth, FACE_SOUTH, elemCenterX - halfX, elemSizeX, false, elemCenterY + halfY },
	};

	auto ensureSlot = [&]() -> TArray<TObjectPtr<UStaticMeshComponent>>& {
		return state.nearComponentsByFloor[floorIndex];
		};

	for (const FFaceInfo& face : faces) {
		if (!face.enabled || face.horizSpan <= 0.f) continue;

		vector<FBuildingWallOpening> openings;
		auto collect = [&](const unordered_map<int, vector<array<float, 8>>>& holes) {
			auto it = holes.find(face.dir);
			if (it == holes.end()) return;
			for (const array<float, 8>& p : it->second) {
				FBuildingWallOpening opening;
				opening.x1 = p[0] * face.horizSpan + p[1];
				opening.x2 = p[4] * face.horizSpan + p[5];
				opening.y1 = p[2] * floorHeight + p[3];
				opening.y2 = p[6] * floorHeight + p[7];
				openings.push_back(opening);
			}
			};
		collect(doors);
		collect(windows);

		// 按沿墙方向的localX/localY(alongY决定跨墙坐标固定用acrossFixed，沿墙坐标用segCenter)
		// 生成一段墙体cube，zBot/zTop是世界坐标——照抄老工程BuildingBase.cpp::ConstructQuad
		// 的makePos/makeSize思路，只是这次每段都是独立组件，不是往同一个PMC里追加顶点。
		auto spawnWallBox = [&](float segStart, float segEnd, float zBot, float zTop) {
			if (segEnd <= segStart || zTop <= zBot) return;
			float segCenter = (segStart + segEnd) * 0.5f;
			float segLen = segEnd - segStart;
			float localX = face.alongY ? face.acrossFixed : segCenter;
			float localY = face.alongY ? segCenter : face.acrossFixed;
			float wx, wy;
			ComputeWorldPosition(*building, localX, localY, wx, wy);
			float sizeXWorld = (face.alongY ? BUILDING_WALL_THICKNESS : segLen) * BUILDING_WORLD_SCALE;
			float sizeYWorld = (face.alongY ? segLen : BUILDING_WALL_THICKNESS) * BUILDING_WORLD_SCALE;
			float wz = (zBot + zTop) * 0.5f;
			float sizeZWorld = zTop - zBot;
			UStaticMeshComponent* comp = SpawnCube(wx, wy, wz, sizeXWorld, sizeYWorld, sizeZWorld, rotation, wallMaterial);
			if (comp) ensureSlot().Add(comp);
			};

		if (openings.empty()) {
			spawnWallBox(face.horizBase, face.horizBase + face.horizSpan, floorBottomWorldZ, floorTopWorldZ);
			continue;
		}

		sort(openings.begin(), openings.end(),
			[](const FBuildingWallOpening& a, const FBuildingWallOpening& b) { return a.x1 < b.x1; });

		float prevX = 0.f;
		for (const FBuildingWallOpening& opening : openings) {
			float segW = opening.x1 - prevX;
			if (segW > 0.f) {
				spawnWallBox(face.horizBase + prevX, face.horizBase + opening.x1, floorBottomWorldZ, floorTopWorldZ);
			}
			// y1/y2照抄老工程BuildingBase.cpp::ConstructQuad::processFace的约定——从天花板往下
			// 量，不是从地板往上量：y1是"开口顶到天花板"这段实心墙(过梁/门楣)的厚度，直接贴着
			// 天花板往下铺；y2是"开口底到天花板"的距离，(floorHeight-y2)才是"开口底到地板"这段
			// 实心墙(门槛/窗台)的厚度，贴着地板往上铺。之前这里的分支写反了(把y1当成贴地板的
			// 门槛、把floorHeight-y2当成贴天花板的过梁)，门(y1≈0/y2=floorHeight，几乎顶到天花板)
			// 因此被画成"底部一段很高的实心墙+顶部完全打通"，效果和预期正好上下颠倒(PIE验证
			// 发现)。
			if (opening.y1 > 0.f) {
				spawnWallBox(face.horizBase + opening.x1, face.horizBase + opening.x2,
					floorTopWorldZ - opening.y1 * BUILDING_WORLD_SCALE, floorTopWorldZ);
			}
			float thresholdH = floorHeight - opening.y2;
			if (thresholdH > 0.f) {
				spawnWallBox(face.horizBase + opening.x1, face.horizBase + opening.x2,
					floorBottomWorldZ, floorBottomWorldZ + thresholdH * BUILDING_WORLD_SCALE);
			}
			prevX = opening.x2;
		}
		float trailingW = face.horizSpan - prevX;
		if (trailingW > 0.f) {
			spawnWallBox(face.horizBase + prevX, face.horizBase + face.horizSpan, floorBottomWorldZ, floorTopWorldZ);
		}
		// 窗户这次不摆任何网格——窗户资产有问题，用户明确要求直接删掉窗户显示逻辑，只保留
		// 开洞几何(和门一样，只是墙上的一个缺口，不生成任何东西)。
	}
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
	if (floorIndex < 0 || floorIndex >= state.nearFloorCount) return;

	int32 level = floorIndex - building->GetBasementCount();
	const Floor* floor = building->GetFloor(level);
	if (!floor) return;

	// floorBaseZ(地图单位，相对地坪grade、不含BUILDING_HEIGHT_EPSILON这个纯渲染层偏移)直接
	// 用Core侧现成的GetFloorBaseZ()，不用像ComputeFloorZRange那样另外反推——两者算的是
	// 同一个值(grade-相对的楼层底部高度)，见Building::GetFloorBaseZ()注释。
	float floorBaseZ = building->GetFloorBaseZ(level);
	float floorHeight = building->GetFloorHeights()[floorIndex];

	// 材质/网格：按mod在AssignFloor里给这一层指定的FloorAssetSpec，留空用组件默认。
	UMaterialInstanceDynamic* wallMaterial = defaultWallMaterial;
	UMaterialInstanceDynamic* floorMaterial = defaultFloorMaterial;
	UMaterialInstanceDynamic* ceilingMaterial = defaultCeilingMaterial;
	UStaticMesh* stairMesh = defaultStairMesh;
	UStaticMesh* rampMesh = defaultRampMesh;
	if (BuildingMod* mod = building->GetMod()) {
		auto specIt = mod->floors.find(level);
		if (specIt != mod->floors.end()) {
			const FloorAssetSpec& assets = specIt->second.assets;
			wallMaterial = ResolveMaterial(assets.wallMaterial, defaultWallMaterial);
			floorMaterial = ResolveMaterial(assets.floorMaterial, defaultFloorMaterial);
			ceilingMaterial = ResolveMaterial(assets.ceilingMaterial, defaultCeilingMaterial);
			stairMesh = ResolveMesh(assets.stairMeshPath, defaultStairMesh);
			rampMesh = ResolveMesh(assets.rampMeshPath, defaultRampMesh);
		}
	}

	float rotation = building->GetRotation();
	auto& slot = state.nearComponentsByFloor[floorIndex];

	// 楼梯/坡道：井道墙体(和corridor/single/row同一套BuildWallsForElement)+按该层资产指定的
	// 3D网格摆一个实体；电梯只有井道墙体，不摆网格(这次不做轿厢，见building.md)。
	for (const Stair& stair : floor->GetStairs()) {
		BuildWallsForElement(building, floorBaseZ, floorHeight, stair.GetPosX(), stair.GetPosY(),
			stair.GetSizeX(), stair.GetSizeY(),
			stair.GetWall(FACE_WEST), stair.GetWall(FACE_EAST), stair.GetWall(FACE_NORTH), stair.GetWall(FACE_SOUTH),
			{}, {}, wallMaterial, state, floorIndex);
		float wx, wy;
		ComputeWorldPosition(*building, stair.GetPosX(), stair.GetPosY(), wx, wy);
		float wz = floorBaseZ * BUILDING_WORLD_SCALE + BUILDING_HEIGHT_EPSILON;
		// 楼梯网格按.layout模板里Stair实际声明的footprint尺寸缩放(X/Y)，Z缩放到整层高度
		// (楼梯本来就是连接上下相邻楼层的)——地图单位换算成世界单位后再传给SpawnMesh；朝向
		// 除了building自身的世界旋转，还要叠加这个楼梯自己的局部direction(west/east方向要
		// 交换X/Y)，见ComputeDirectionalMeshTransform注释。
		float meshSizeX, meshSizeY, meshRotation;
		ComputeDirectionalMeshTransform(stair.GetDirection(), stair.GetSizeX(), stair.GetSizeY(), rotation,
			meshSizeX, meshSizeY, meshRotation);
		if (UStaticMeshComponent* comp = SpawnMesh(wx, wy, wz,
			meshSizeX * BUILDING_WORLD_SCALE, meshSizeY * BUILDING_WORLD_SCALE,
			floorHeight * BUILDING_WORLD_SCALE, meshRotation, stairMesh, nullptr)) {
			slot.Add(comp);
		}
	}
	for (const Ramp& ramp : floor->GetRamps()) {
		BuildWallsForElement(building, floorBaseZ, floorHeight, ramp.GetPosX(), ramp.GetPosY(),
			ramp.GetSizeX(), ramp.GetSizeY(),
			ramp.GetWall(FACE_WEST), ramp.GetWall(FACE_EAST), ramp.GetWall(FACE_NORTH), ramp.GetWall(FACE_SOUTH),
			{}, {}, wallMaterial, state, floorIndex);
		float wx, wy;
		ComputeWorldPosition(*building, ramp.GetPosX(), ramp.GetPosY(), wx, wy);
		float wz = floorBaseZ * BUILDING_WORLD_SCALE + BUILDING_HEIGHT_EPSILON;
		// 同上：坡道网格按Ramp实际声明的footprint尺寸(X/Y)+整层高度(Z)缩放，朝向叠加自己的
		// 局部direction。
		float meshSizeX, meshSizeY, meshRotation;
		ComputeDirectionalMeshTransform(ramp.GetDirection(), ramp.GetSizeX(), ramp.GetSizeY(), rotation,
			meshSizeX, meshSizeY, meshRotation);
		if (UStaticMeshComponent* comp = SpawnMesh(wx, wy, wz,
			meshSizeX * BUILDING_WORLD_SCALE, meshSizeY * BUILDING_WORLD_SCALE,
			floorHeight * BUILDING_WORLD_SCALE, meshRotation, rampMesh, nullptr)) {
			slot.Add(comp);
		}
	}
	for (const Elevator& elevator : floor->GetElevators()) {
		BuildWallsForElement(building, floorBaseZ, floorHeight, elevator.GetPosX(), elevator.GetPosY(),
			elevator.GetSizeX(), elevator.GetSizeY(),
			elevator.GetWall(FACE_WEST), elevator.GetWall(FACE_EAST), elevator.GetWall(FACE_NORTH), elevator.GetWall(FACE_SOUTH),
			{}, {}, wallMaterial, state, floorIndex);
	}
	for (const Corridor& corridor : floor->GetCorridors()) {
		BuildWallsForElement(building, floorBaseZ, floorHeight, corridor.GetPosX(), corridor.GetPosY(),
			corridor.GetSizeX(), corridor.GetSizeY(),
			corridor.GetWall(FACE_WEST), corridor.GetWall(FACE_EAST), corridor.GetWall(FACE_NORTH), corridor.GetWall(FACE_SOUTH),
			corridor.GetDoors(), corridor.GetWindows(), wallMaterial, state, floorIndex);
	}
	// Single/Row槽位隐含四面都有墙——但槽位本身在实例化成真正的Room之后，门/窗/朝向都已经
	// 转移到Room身上(见Building::AssignRoom/ArrangeRow)，槽位自己的门窗数据不再是最新的，
	// 这里改成遍历building->GetRooms()按GetLayer()==level筛选，画每个真正Room的墙体，
	// 不直接用Floor::GetSingles()/GetRows()（那两个列表只是"模板槽位"，槽位数量/大小和实际
	// 生成的Room数量不是一一对应——ArrangeRow会把一个row槽位切成好几个Room）。
	for (Room* room : building->GetRooms()) {
		if (!room || room->GetLayer() != level) continue;
		BuildWallsForElement(building, floorBaseZ, floorHeight, room->GetPosX(), room->GetPosY(),
			room->GetSizeX(), room->GetSizeY(),
			true, true, true, true, // Single/Row隐含四面都有墙
			room->GetDoors(), room->GetWindows(), wallMaterial, state, floorIndex);
	}

	// 地板/天花板：老工程两层独立薄slab的做法——地板贴地坪往上一点点，天花板贴楼层顶往下
	// 一点点，中间留一条很窄的缝，避免和上/下相邻楼层的对应slab共面z-fighting。
	constexpr float kSlabThickness = 0.02f; // 地图单位，照抄老工程Ceiling/Ground的0.02f厚度
	float floorBottomWorld = BUILDING_HEIGHT_EPSILON + floorBaseZ * BUILDING_WORLD_SCALE;
	float floorTopWorld = BUILDING_HEIGHT_EPSILON + (floorBaseZ + floorHeight) * BUILDING_WORLD_SCALE;
	for (const Ground& ground : floor->GetGrounds()) {
		float wx, wy;
		ComputeWorldPosition(*building, ground.GetPosX(), ground.GetPosY(), wx, wy);
		float wz = floorBottomWorld + kSlabThickness * 0.5f * BUILDING_WORLD_SCALE;
		float sizeX = ground.GetSizeX() * BUILDING_WORLD_SCALE, sizeY = ground.GetSizeY() * BUILDING_WORLD_SCALE;
		float sizeZ = kSlabThickness * BUILDING_WORLD_SCALE;
		if (UStaticMeshComponent* comp = SpawnCube(wx, wy, wz, sizeX, sizeY, sizeZ, rotation, floorMaterial)) {
			slot.Add(comp);
		}
	}
	for (const Ceiling& ceiling : floor->GetCeilings()) {
		float wx, wy;
		ComputeWorldPosition(*building, ceiling.GetPosX(), ceiling.GetPosY(), wx, wy);
		float wz = floorTopWorld - kSlabThickness * 0.5f * BUILDING_WORLD_SCALE;
		float sizeX = ceiling.GetSizeX() * BUILDING_WORLD_SCALE, sizeY = ceiling.GetSizeY() * BUILDING_WORLD_SCALE;
		float sizeZ = kSlabThickness * BUILDING_WORLD_SCALE;
		if (UStaticMeshComponent* comp = SpawnCube(wx, wy, wz, sizeX, sizeY, sizeZ, rotation, ceilingMaterial)) {
			slot.Add(comp);
		}
	}
}

void UForeverBuildingFrameworkComponent::ClearNearSections(Building* building, FBuildingRenderState& state) {
	AActor* owner = GetOwner();
	for (TArray<TObjectPtr<UStaticMeshComponent>>& floorComponents : state.nearComponentsByFloor) {
		for (UStaticMeshComponent* comp : floorComponents) {
			if (!comp) continue;
			if (owner) owner->RemoveInstanceComponent(comp);
			comp->DestroyComponent();
		}
		floorComponents.Empty();
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
