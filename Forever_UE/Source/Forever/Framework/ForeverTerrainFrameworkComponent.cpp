#include "Framework/ForeverTerrainFrameworkComponent.h"

#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/TerrainMaterialBuilder.h"
#include "UObject/ConstructorHelpers.h"

#include "map/map.h"
#include "map/general_geometry.h"

#define HEIGHT_EPSILON 0.002f
// 顶点法线用高度场中心差分求，差分步长(element)：各级LOD统一，大于L0顶点间距(1/16)以抹开单个格点的台阶感。
#define NORMAL_SAMPLE_STEP 0.25f
// 每级LOD mesh的网格细分数(每级都是这个数,级与级之间只有size不一样；64即65x65个顶点，128实测太卡)。
// BuildLevel里所有跟它绑定的数(顶点数、行跨度、父子边界偏移)都由它推导，调这一个数即可。
#define TERRAIN_LOD_GRID_SUBDIV 64
// 显示hatch(挖洞)的最远LOD级别(含)：L0~L4的网格小格边长是1/16、1/8、1/4、1/2、1 element，都不跨element(L5起小格跨element，不能直接逐格裁剪)，
// 可以逐格对三角形做hatch裁剪；construction格子的精确几何仍然只在L0/L1做。
#define TERRAIN_HATCH_MAX_LEVEL 4
// L0覆盖4个element、64段，每个element正好16段=TERRAIN_FINE_SUBDIV，各级LOD顶点都落在细化高度
// 网格点上，Map::SampleHeight直接查表；改了这个数也不会出错，只是L0顶点落到网格点之间，改为插值。

using namespace std;

UForeverTerrainFrameworkComponent::UForeverTerrainFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> tunnelWallMatFinder(
		TEXT("/Game/Asset/Common/Material/PureColor.PureColor"));
	if (tunnelWallMatFinder.Succeeded()) {
		tunnelWallBaseMaterial = tunnelWallMatFinder.Object;
	}
}

void UForeverTerrainFrameworkComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!map) return;
	if (currentPivots.empty()) return;
	// 世界还在分阶段生成：地形LOD环要围着真正的玩家(game_start/读档指定的市民)转，不是过渡用的DefaultPawn。
	if (const AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(GetOwner())) {
		if (!frameworkActor->IsInitialGenerationDone()) return;
	}

	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn) return;

	FVector location = pawn->GetActorLocation() / 1000.f;
	auto mapSize = map->GetSize();

	int numLevels = (int)currentPivots.size();
	vector<pair<int, int>> newPivots(currentPivots.size());
	for (int i = 0; i < numLevels; i++) {
		auto half = currentPivots[i].second / 2.f;
		int rawX = static_cast<int>((location.X + half / 2.f) / half);
		int rawY = static_cast<int>((location.Y + half / 2.f) / half);

		// 限制pivot范围,避免mesh覆盖区域越出地图边界
		int minPivot = 1;
		int maxPivotX = FMath::Max(minPivot, FMath::FloorToInt(mapSize.first / half) - 1);
		int maxPivotY = FMath::Max(minPivot, FMath::FloorToInt(mapSize.second / half) - 1);

		newPivots[i].first = FMath::Clamp(rawX, minPivot, maxPivotX);
		newPivots[i].second = FMath::Clamp(rawY, minPivot, maxPivotY);
	}

	// boundaryChanged[i]: 当级mesh被重建后其边界顶点是否会改变。从粗到细传播:父级边界变->子级接缝也变
	vector<bool> boundaryChanged(numLevels, false);
	boundaryChanged[numLevels - 1] = (newPivots[numLevels - 1] != currentPivots[numLevels - 1].first);
	for (int i = numLevels - 2; i >= 0; i--)
		boundaryChanged[i] = (newPivots[i] != currentPivots[i].first) || boundaryChanged[i + 1];

	// needsRebuild[i]: 自身pivot变、子级移动(挖洞位置变)、父级边界变(接缝修正变)
	vector<bool> needsRebuild(numLevels, false);
	for (int i = 0; i < numLevels; i++) {
		if (newPivots[i] != currentPivots[i].first) needsRebuild[i] = true;
		if (i > 0 && newPivots[i - 1] != currentPivots[i - 1].first) needsRebuild[i] = true;
		if (i < numLevels - 1 && boundaryChanged[i + 1]) needsRebuild[i] = true;
	}

	// 从粗到细重建,让父级已修正的边界数据传递给子级
	for (int i = numLevels - 1; i >= 0; i--) {
		if (!needsRebuild[i]) continue;

		int childPos = 0;
		if (i >= 1) {
			int idx = (newPivots[i - 1].second + 1 - newPivots[i].second * 2) * 3 +
				(newPivots[i - 1].first + 1 - newPivots[i].first * 2);
			switch (idx) {
			case 0: childPos = 0xCC00; break;
			case 1: childPos = 0x6600; break;
			case 2: childPos = 0x3300; break;
			case 3: childPos = 0x0CC0; break;
			case 4: childPos = 0x0660; break;
			case 5: childPos = 0x0330; break;
			case 6: childPos = 0x00CC; break;
			case 7: childPos = 0x0066; break;
			case 8: childPos = 0x0033; break;
			default: break;
			}
		}

		// 计算与父级(i+1)的边界重合标志和偏移量
		int edgeFlags = 0, parentOffsetX = 0, parentOffsetY = 0;
		if (i < numLevels - 1) {
			int idxX = newPivots[i].first + 1 - newPivots[i + 1].first * 2;
			int idxY = newPivots[i].second + 1 - newPivots[i + 1].second * 2;
			if (idxX == 0) edgeFlags |= 0x1;
			if (idxX == 2) edgeFlags |= 0x2;
			if (idxY == 0) edgeFlags |= 0x4;
			if (idxY == 2) edgeFlags |= 0x8;
			parentOffsetX = idxX * (TERRAIN_LOD_GRID_SUBDIV / 4);
			parentOffsetY = idxY * (TERRAIN_LOD_GRID_SUBDIV / 4);
		}

		const FForeverLodBoundary* parentBoundary = (i < numLevels - 1) ? &lodBoundaries[i + 1] : nullptr;
		BuildLevel(i, newPivots[i], currentPivots[i].second, childPos,
			parentBoundary, parentOffsetX, parentOffsetY, edgeFlags);
		currentPivots[i].first = newPivots[i];
	}
}

void UForeverTerrainFrameworkComponent::GenerateTerrain(Map* inMap) {
	map = inMap;
	if (!map) return;

	AActor* owner = GetOwner();
	if (!owner) return;

	auto size = map->GetSize();
	auto target = max(size.first, size.second);
	int num = static_cast<int>(log2(static_cast<double>(target))) - 1;
	if (num < 1) num = 1;
	currentPivots.assign(num, {});
	for (int i = 0; i < num; i++) {
		currentPivots[i] = { { INT_MIN, INT_MIN }, 4.f * powf(2.f, static_cast<float>(i)) };
	}

	// 材质：层贴图数组/覆盖图/表/噪声都在这里一次性生成，之后不再重建。基础材质M_Terrain由Tools/TerrainMaterial/build_terrain_material.py生成；
	// 这里运行时才加载(不在构造函数里FObjectFinder)，否则编辑器里跑那个脚本重建材质时它已被CDO引用着，清节点会断言。找不到就是没有地形材质(Build打Error)。
	if (!terrainBaseMaterial) {
		terrainBaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Asset/Terrain/M_Terrain.M_Terrain"));
	}
	terrainMaterial = FTerrainMaterialBuilder::Build(this, map, terrainBaseMaterial);
	{
		const auto [mapWidth, mapHeight] = map->GetSize();
		waterFieldWidth = mapWidth;
		FTerrainMaterialBuilder::BuildWaterLevelField(map, 6, waterLevelField);
	}

	gridMeshes.SetNum(num);
	for (int i = 0; i < num; i++) {
		UProceduralMeshComponent* comp = NewObject<UProceduralMeshComponent>(
			owner, *FString::Printf(TEXT("TerrainLod_%d"), i));
		comp->SetupAttachment(owner->GetRootComponent());
		comp->SetCastShadow(false);
		comp->RegisterComponent();
		owner->AddInstanceComponent(comp);
		gridMeshes[i] = comp;
	}

	lodBoundaries.resize(num);
	for (auto& boundary : lodBoundaries) {
		boundary.bottom.SetNum(TERRAIN_LOD_GRID_SUBDIV + 1);
		boundary.top.SetNum(TERRAIN_LOD_GRID_SUBDIV + 1);
		boundary.left.SetNum(TERRAIN_LOD_GRID_SUBDIV + 1);
		boundary.right.SetNum(TERRAIN_LOD_GRID_SUBDIV + 1);
	}

	BuildTunnelWalls();
}

float UForeverTerrainFrameworkComponent::SampleHeight(float mapX, float mapY) const {
	// 粗高度双线性+细化高度数组查表都在Core的Map::SampleHeight里，见map.md"细化高度"一节。
	return map ? map->SampleHeight(mapX, mapY) : 0.f;
}

bool UForeverTerrainFrameworkComponent::IsInsideLodLevel(const FVector& worldPos, int32 level, float scale) const {
	if (level < 0 || level >= static_cast<int32>(currentPivots.size())) return false;
	const auto& entry = currentPivots[level];
	if (entry.first.first == INT_MIN) return false; // 这一级还没建过
	// 和TickComponent里算originX的公式一致：区域左下角 = pivot*(size/2) - size/2，边长size（地图元素单位）。
	const float size = entry.second;
	const float originX = entry.first.first * (size / 2.f) - size / 2.f;
	const float originY = entry.first.second * (size / 2.f) - size / 2.f;
	const float centerX = originX + size / 2.f;
	const float centerY = originY + size / 2.f;
	const float half = size / 2.f * scale;
	const float x = worldPos.X / 1000.f;
	const float y = worldPos.Y / 1000.f;
	return FMath::Abs(x - centerX) <= half && FMath::Abs(y - centerY) <= half;
}

FVector UForeverTerrainFrameworkComponent::GetMapCenterWorldLocation() const {
	if (!map) return FVector::ZeroVector;

	auto size = map->GetSize();
	const float worldScale = 1000.f;
	float centerX = size.first / 2.f;
	float centerY = size.second / 2.f;
	float height = SampleHeight(centerX, centerY);

	// 留出2m安全余量,避免玩家出生时卡进地形
	return FVector(centerX * worldScale, centerY * worldScale, height * worldScale + 200.f);
}

namespace {
	// 多边形面积的绝对值(鞋带公式)。裁剪时一个顶点正好落在裁剪线上(side==0)、或相邻两个hatch共边时，会切出
	// 重复点/共线点的零面积碎片(Num()>=3但面积为0)，画出来就是退化三角形，Chaos建碰撞体时报
	// "Input trimesh contains N bad triangles"，所以收下碎片前要先用面积过滤掉。
	template <typename T, typename GetPos>
	float PolygonAreaAbs(const TArray<T>& polygon, GetPos getPos) {
		double twice = 0.0;
		for (int32 i = 0; i < polygon.Num(); i++) {
			auto a = getPos(polygon[i]);
			auto b = getPos(polygon[(i + 1) % polygon.Num()]);
			twice += (double)a.X * b.Y - (double)b.X * a.Y;
		}
		return (float)(FMath::Abs(twice) * 0.5);
	}

	// clipGridCell在世界坐标(UE单位)里裁剪：坐标在几十万量级，float精度只有0.03~0.06，共边/过顶点的hatch
	// 切出的碎片宽度就是这个量级的噪声，面积在10 cm2以内；而一个小格是几千cm2。
	constexpr float kMinWorldPieceArea = 10.f;
	// 单个扇形三角形的最小面积(同上世界坐标)：重复点/共线点产生的零面积三角形直接丢掉。
	constexpr float kMinWorldTriArea = 1.f;

	// 网格小格的三角形被hatch裁剪时用的多边形顶点：pos是世界坐标(已经含LOD接缝修正和HEIGHT_EPSILON，
	// 即这个三角形真正渲染的位置)，index是它在BuildLevel里vertices数组中的下标——三角形原有的
	// 角点直接复用网格顶点(和邻格共用，没有裂缝、法线连续)，裁剪新产生的交点index=-1，输出时才追加。
	struct FClipPoint {
		FVector pos;
		int32 index;
	};

	// 用有向线段把凸多边形分成两部分：交点在三角形所在平面上做3D线性插值(原三角形是
	// 平面，边是直线，所以交点的高度正好落在原来的渲染表面上，不会漂)，侧判断只看XY。
	void SplitClipPolygonByLine(const TArray<FClipPoint>& polygon, const FVector2D& edgeA, const FVector2D& edgeB,
		const FVector2D& referencePoint, TArray<FClipPoint>& outInside, TArray<FClipPoint>& outOutside) {
		outInside.Reset();
		outOutside.Reset();
		int32 n = polygon.Num();
		if (n < 3) return;

		FVector2D dir = edgeB - edgeA;
		auto side = [&](const FClipPoint& p) { return dir.X * (p.pos.Y - edgeA.Y) - dir.Y * (p.pos.X - edgeA.X); };
		bool refPositive = (dir.X * (referencePoint.Y - edgeA.Y) - dir.Y * (referencePoint.X - edgeA.X)) >= 0.f;

		for (int32 i = 0; i < n; i++) {
			const FClipPoint& cur = polygon[i];
			const FClipPoint& next = polygon[(i + 1) % n];
			float curSide = side(cur);
			float nextSide = side(next);
			bool curInside = (curSide >= 0.f) == refPositive;
			bool nextInside = (nextSide >= 0.f) == refPositive;

			if (curInside) outInside.Add(cur); else outOutside.Add(cur);

			if (curInside != nextInside) {
				float t = curSide / (curSide - nextSide);
				FClipPoint intersection{ cur.pos + (next.pos - cur.pos) * t, -1 };
				outInside.Add(intersection);
				outOutside.Add(intersection);
			}
		}
	}

}

void UForeverTerrainFrameworkComponent::BuildLevel(int levelIdx, pair<int, int> pos, float size, int childPos,
	const FForeverLodBoundary* parentBoundary, int parentOffsetX, int parentOffsetY, int edgeFlags) {
	if (!map) return;

	auto mapSize = map->GetSize();
	UProceduralMeshComponent* mesh = gridMeshes[levelIdx];

	const float cellSize = size / (float)TERRAIN_LOD_GRID_SUBDIV;
	const float startX = pos.first * (size / 2.f) - size / 2.f;
	const float startY = pos.second * (size / 2.f) - size / 2.f;
	const float worldScale = 1000.f;

	// 材质不读网格UV,全部用世界坐标(AbsoluteWorldPosition/1000=地图element坐标)采样,
	// 所以CreateMeshSection不传UV,见.md"地形材质"一节。
	TArray<FVector> vertices;
	TArray<int32> triangles;

	vertices.Reserve((TERRAIN_LOD_GRID_SUBDIV + 1) * (TERRAIN_LOD_GRID_SUBDIV + 1));

	for (int vy = 0; vy <= TERRAIN_LOD_GRID_SUBDIV; vy++) {
		for (int vx = 0; vx <= TERRAIN_LOD_GRID_SUBDIV; vx++) {
			float mapX = startX + vx * cellSize;
			float mapY = startY + vy * cellSize;
			float h = SampleHeight(mapX, mapY) + HEIGHT_EPSILON;

			vertices.Add(FVector(mapX * worldScale, mapY * worldScale, h * worldScale));
		}
	}

	auto vertIdx = [](int vx, int vy) { return vy * (TERRAIN_LOD_GRID_SUBDIV + 1) + vx; };

	// 若与父级(粗一级)LOD存在共享边,先将偶数顶点对齐到父级已修正的边界值,
	// 再统一做奇数插值,确保多级边界完全一致
	if (parentBoundary && edgeFlags) {
		if (edgeFlags & 0x1) // 左边(vx=0)与父级左边重合
			for (int k = 0; k <= TERRAIN_LOD_GRID_SUBDIV; k += 2)
				vertices[vertIdx(0, k)] = parentBoundary->left[parentOffsetY + k / 2];
		if (edgeFlags & 0x2) // 右边(vx=TERRAIN_LOD_GRID_SUBDIV)与父级右边重合
			for (int k = 0; k <= TERRAIN_LOD_GRID_SUBDIV; k += 2)
				vertices[vertIdx(TERRAIN_LOD_GRID_SUBDIV, k)] = parentBoundary->right[parentOffsetY + k / 2];
		if (edgeFlags & 0x4) // 下边(vy=0)与父级下边重合
			for (int k = 0; k <= TERRAIN_LOD_GRID_SUBDIV; k += 2)
				vertices[vertIdx(k, 0)] = parentBoundary->bottom[parentOffsetX + k / 2];
		if (edgeFlags & 0x8) // 上边(vy=TERRAIN_LOD_GRID_SUBDIV)与父级上边重合
			for (int k = 0; k <= TERRAIN_LOD_GRID_SUBDIV; k += 2)
				vertices[vertIdx(k, TERRAIN_LOD_GRID_SUBDIV)] = parentBoundary->top[parentOffsetX + k / 2];
	}

	// 让边缘在粗一级LOD的格点间距上保持分段线性,消除接缝裂缝
	for (int edgeVy : { 0, TERRAIN_LOD_GRID_SUBDIV })
		for (int vx = 1; vx < TERRAIN_LOD_GRID_SUBDIV; vx += 2)
			vertices[vertIdx(vx, edgeVy)] = (vertices[vertIdx(vx - 1, edgeVy)] + vertices[vertIdx(vx + 1, edgeVy)]) * 0.5f;
	for (int edgeVx : { 0, TERRAIN_LOD_GRID_SUBDIV })
		for (int vy = 1; vy < TERRAIN_LOD_GRID_SUBDIV; vy += 2)
			vertices[vertIdx(edgeVx, vy)] = (vertices[vertIdx(edgeVx, vy - 1)] + vertices[vertIdx(edgeVx, vy + 1)]) * 0.5f;

	// 有hatch的element里(construction格子也一样)，一个网格小格(cx,cy)的两个三角形按这个element的全部
	// hatch旋转矩形裁剪，洞里的部分丢掉，洞外的部分用原三角形的平面插值出交点(高度和原渲染表面一致)，
	// 角点复用网格顶点。小格不会跨element(L0/L1的小格边长是1/16、1/8 element，网格和element对齐)。
	// 三角形和BuildLevel主循环同一对角线、同一绕序：(v00,v10,v11)/(v00,v11,v01)是CCW，扇形输出
	// (p0,p[i+1],p[i])就是主循环里的(v00,v11,v10)/(v00,v01,v11)。
	auto clipGridCell = [this, &vertices, &triangles, worldScale](int cx, int cy, int ex, int ey, bool anti) {
		const auto& hatches = map->GetHatches(ex, ey);
		int32 v00 = cy * (TERRAIN_LOD_GRID_SUBDIV + 1) + cx;
		int32 v10 = v00 + 1;
		int32 v01 = v00 + (TERRAIN_LOD_GRID_SUBDIV + 1);
		int32 v11 = v01 + 1;
		// 对角线随这个小格的自适应选择(anti=副对角线v10-v01)，和主循环一致。
		const int32 cellTrisMain[2][3] = { { v00, v10, v11 }, { v00, v11, v01 } };
		const int32 cellTrisAnti[2][3] = { { v00, v10, v01 }, { v10, v11, v01 } };
		const int32 (*cellTris)[3] = anti ? cellTrisAnti : cellTrisMain;

		for (int t = 0; t < 2; t++) {
			TArray<FClipPoint> start;
			FVector2D boundsMin(FLT_MAX, FLT_MAX), boundsMax(-FLT_MAX, -FLT_MAX);
			for (int k = 0; k < 3; k++) {
				const FVector& p = vertices[cellTris[t][k]];
				start.Add({ p, cellTris[t][k] });
				boundsMin.X = FMath::Min(boundsMin.X, p.X); boundsMin.Y = FMath::Min(boundsMin.Y, p.Y);
				boundsMax.X = FMath::Max(boundsMax.X, p.X); boundsMax.Y = FMath::Max(boundsMax.Y, p.Y);
			}

			TArray<TArray<FClipPoint>> workPolys;
			workPolys.Add(start);

			for (const TerrainHatch& terrainHatch : hatches) {
				const Quad& quad = terrainHatch.quad;
				float rotation = terrainHatch.rotation;
				if (workPolys.Num() == 0) break; // 这个三角形已经被挖空了

				FVector2D hatchCenter(quad.GetPosX() * worldScale, quad.GetPosY() * worldScale);
				float halfSizeX = quad.GetSizeX() * 0.5f * worldScale;
				float halfSizeY = quad.GetSizeY() * 0.5f * worldScale;
				float cosRot = FMath::Cos(rotation), sinRot = FMath::Sin(rotation);

				// 旋转矩形的外接矩形和这个三角形的外接矩形不重叠就直接跳过。
				float aabbHalfX = FMath::Abs(halfSizeX * cosRot) + FMath::Abs(halfSizeY * sinRot);
				float aabbHalfY = FMath::Abs(halfSizeX * sinRot) + FMath::Abs(halfSizeY * cosRot);
				if (hatchCenter.X + aabbHalfX <= boundsMin.X || hatchCenter.X - aabbHalfX >= boundsMax.X) continue;
				if (hatchCenter.Y + aabbHalfY <= boundsMin.Y || hatchCenter.Y - aabbHalfY >= boundsMax.Y) continue;

				FVector2D hatchPoly[4] = {
					{ hatchCenter.X + halfSizeX * cosRot - halfSizeY * sinRot, hatchCenter.Y + halfSizeX * sinRot + halfSizeY * cosRot },
					{ hatchCenter.X - halfSizeX * cosRot - halfSizeY * sinRot, hatchCenter.Y - halfSizeX * sinRot + halfSizeY * cosRot },
					{ hatchCenter.X - halfSizeX * cosRot + halfSizeY * sinRot, hatchCenter.Y - halfSizeX * sinRot - halfSizeY * cosRot },
					{ hatchCenter.X + halfSizeX * cosRot + halfSizeY * sinRot, hatchCenter.Y + halfSizeX * sinRot - halfSizeY * cosRot },
				};

				// 依次按矩形4条边分割，外侧部分确定在矩形外，收下；
				// 4条边都留在内侧的部分才是洞，丢弃。
				TArray<TArray<FClipPoint>> survivingPieces;
				for (const TArray<FClipPoint>& piece : workPolys) {
					TArray<FClipPoint> currentInside = piece;
					for (int32 e = 0; e < 4 && currentInside.Num() >= 3; e++) {
						TArray<FClipPoint> insidePart, outsidePart;
						SplitClipPolygonByLine(currentInside, hatchPoly[e], hatchPoly[(e + 1) % 4], hatchCenter, insidePart, outsidePart);
						if (outsidePart.Num() >= 3 && PolygonAreaAbs(outsidePart, [](const FClipPoint& p) { return p.pos; }) > kMinWorldPieceArea) {
							survivingPieces.Add(outsidePart);
						}
						currentInside = MoveTemp(insidePart);
					}
				}
				workPolys = MoveTemp(survivingPieces);
			}

			for (TArray<FClipPoint>& piece : workPolys) {
				for (int32 i = 1; i + 1 < piece.Num(); i++) {
					// 多边形里有重复点/共线点时(裁剪线正好过顶点)，扇形里会出零面积三角形，
					// Chaos建碰撞体会报bad triangles，跳过；只给真正用到的交点追加顶点。
					const FVector& a = piece[0].pos; const FVector& b = piece[i].pos; const FVector& c = piece[i + 1].pos;
					double cross = ((double)b.X - a.X) * ((double)c.Y - a.Y) - ((double)c.X - a.X) * ((double)b.Y - a.Y);
					if (FMath::Abs(cross) * 0.5 <= kMinWorldTriArea) continue;
					for (int32 k : { 0, i, i + 1 }) {
						if (piece[k].index < 0) piece[k].index = vertices.Add(piece[k].pos);
					}
					triangles.Add(piece[0].index); triangles.Add(piece[i + 1].index); triangles.Add(piece[i].index);
				}
			}
		}
	};

	const int quadrantSize = TERRAIN_LOD_GRID_SUBDIV / 4; // childPos是4x4(16格)的子级覆盖位掩码,跟细分数无关,一直是4x4
	for (int cy = 0; cy < TERRAIN_LOD_GRID_SUBDIV; cy++) {
		for (int cx = 0; cx < TERRAIN_LOD_GRID_SUBDIV; cx++) {
			int s = (cy / quadrantSize) * 4 + (cx / quadrantSize);
			if ((childPos >> (15 - s)) & 1) continue;

			// 这个小格拆三角形的对角线：和Map::SampleHeight的"自适应对角线"一致，斜着走的地形交界才是一道笔直的斜坡。
			// L0~L4的小格不跨element，没有细化数组的element直接用Map的判断(采样和网格严格同一对角线)；
			// 有细化数组或更粗的LOD(小格跨多个element)就看这个小格4个顶点自己的高度，取高度差较小的那条对角线。
			bool anti;
			{
				const int32 w00 = cy * (TERRAIN_LOD_GRID_SUBDIV + 1) + cx;
				const int32 w01 = w00 + (TERRAIN_LOD_GRID_SUBDIV + 1);
				bool fromMap = false;
				if (levelIdx <= TERRAIN_HATCH_MAX_LEVEL) {
					int dex = FMath::Clamp(FMath::FloorToInt(startX + (cx + 0.5f) * cellSize), 0, mapSize.first - 1);
					int dey = FMath::Clamp(FMath::FloorToInt(startY + (cy + 0.5f) * cellSize), 0, mapSize.second - 1);
					if (!map->GetFineHeights(dex, dey)) { anti = map->UsesAntiDiagonal(dex, dey); fromMap = true; }
				}
				if (!fromMap) anti = FMath::Abs(vertices[w00 + 1].Z - vertices[w01].Z) < FMath::Abs(vertices[w00].Z - vertices[w01 + 1].Z);
			}

			bool drawQuad = true;
			if (levelIdx <= TERRAIN_HATCH_MAX_LEVEL) {
				float quadCenterMapX = startX + (cx + 0.5f) * cellSize;
				float quadCenterMapY = startY + (cy + 0.5f) * cellSize;
				int ex = FMath::Clamp(FMath::FloorToInt(quadCenterMapX), 0, mapSize.first - 1);
				int ey = FMath::Clamp(FMath::FloorToInt(quadCenterMapY), 0, mapSize.second - 1);
				// 被Map::AddHatch记过洞的格子(construction的地下室井口、隧道口等，可能在有起伏的地形上)只对这个小格自己的
				// 两个三角形做hatch裁剪，没被切到的小格仍用共享的网格顶点，不丢细化高度也没有裂缝。
				if (!map->GetHatches(ex, ey).empty()) {
						clipGridCell(cx, cy, ex, ey, anti);
						drawQuad = false;
					}
			}
			if (!drawQuad) continue;

			int32 v00 = cy * (TERRAIN_LOD_GRID_SUBDIV + 1) + cx;
			int32 v10 = cy * (TERRAIN_LOD_GRID_SUBDIV + 1) + cx + 1;
			int32 v01 = (cy + 1) * (TERRAIN_LOD_GRID_SUBDIV + 1) + cx;
			int32 v11 = (cy + 1) * (TERRAIN_LOD_GRID_SUBDIV + 1) + cx + 1;
			if (anti) {
				triangles.Add(v00); triangles.Add(v01); triangles.Add(v10);
				triangles.Add(v10); triangles.Add(v01); triangles.Add(v11);
			} else {
				triangles.Add(v00); triangles.Add(v11); triangles.Add(v10);
				triangles.Add(v00); triangles.Add(v01); triangles.Add(v11);
			}
		}
	}

	// ProceduralMeshComponent不会自动算法线。法线放在顶点上、由GPU在三角形内插值：这里不再用相邻三角形的面法线累加
	// (那样法线取决于三角形怎么拆，对角线一变/顶点不共享——construction整格替换、hatch裁剪交点都是各自独立的顶点——
	// 三角形边界就会出棱角)，而是直接对高度场求梯度：顶点位置处用SampleHeight做中心差分，
	// 法线只取决于位置，所以相邻三角形、各级LOD、独立顶点在同一位置上法线完全一致，插值出来是连续变化的。
	// 差分步长固定为NORMAL_SAMPLE_STEP个element(各级LOD一致，也比L0的1/16顶点间距宽，把单个格点的台阶感抹开)。
	const float normalStep = NORMAL_SAMPLE_STEP;
	TArray<FVector> normals;
	normals.SetNumUninitialized(vertices.Num());
	for (int32 i = 0; i < vertices.Num(); i++) {
		const float mx = vertices[i].X / worldScale, my = vertices[i].Y / worldScale;
		const float dhdx = (SampleHeight(mx + normalStep, my) - SampleHeight(mx - normalStep, my)) / (2.f * normalStep);
		const float dhdy = (SampleHeight(mx, my + normalStep) - SampleHeight(mx, my - normalStep)) / (2.f * normalStep);
		normals[i] = FVector(-dhdx, -dhdy, 1.f).GetSafeNormal(); // XY(地图element)和Z(高度)同单位
	}
	TArray<FProcMeshTangent> tangents;
	tangents.SetNumUninitialized(vertices.Num());
	for (int32 i = 0; i < normals.Num(); i++) {
		FVector n = normals[i].GetSafeNormal();
		if (n.IsNearlyZero()) n = FVector::UpVector;
		normals[i] = n;
		FVector t = (FVector::ForwardVector - n * FVector::DotProduct(FVector::ForwardVector, n)).GetSafeNormal();
		if (t.IsNearlyZero()) t = FVector::RightVector;
		tangents[i] = FProcMeshTangent(t, false);
	}

	// 顶点色R=水深(最近水面高度-地面高度)，材质据此给河/海/沙滩按水线分层。位置决定值，和LOD、三角形怎么拆无关；
	// hatch裁剪新增的交点顶点也按自己的位置算。GBA未用。
	TArray<FColor> vertexColors;
	vertexColors.SetNumUninitialized(vertices.Num());
	for (int32 i = 0; i < vertices.Num(); i++) {
		const float mx = vertices[i].X / worldScale, my = vertices[i].Y / worldScale;
		const float depth = SampleWaterDepthMeters(mx, my, vertices[i].Z / worldScale);
		vertexColors[i] = FColor(FTerrainMaterialBuilder::EncodeWaterDepth(depth), 0, 0, 255);
	}

	mesh->CreateMeshSection(0, vertices, triangles,
		normals, TArray<FVector2D>(), vertexColors, tangents, true);

	// 保存修正后的四条边界顶点,供更细一级LOD进行接缝修正时使用
	if (levelIdx < (int)lodBoundaries.size()) {
		FForeverLodBoundary& boundary = lodBoundaries[levelIdx];
		for (int k = 0; k <= TERRAIN_LOD_GRID_SUBDIV; k++) {
			boundary.bottom[k] = vertices[vertIdx(k, 0)];
			boundary.top[k] = vertices[vertIdx(k, TERRAIN_LOD_GRID_SUBDIV)];
			boundary.left[k] = vertices[vertIdx(0, k)];
			boundary.right[k] = vertices[vertIdx(TERRAIN_LOD_GRID_SUBDIV, k)];
		}
	}

	if (terrainMaterial) {
		mesh->SetMaterial(0, terrainMaterial);
	}
}

float UForeverTerrainFrameworkComponent::SampleWaterDepthMeters(float mapX, float mapY, float height) const {
	const float none = FTerrainMaterialBuilder::WaterDepthMinMeters;
	if (waterLevelField.empty() || waterFieldWidth <= 0 || !map) return none;
	const auto [width, mapHeight] = map->GetSize();
	const int32 ex = FMath::Clamp(FMath::FloorToInt(mapX), 0, width - 1);
	const int32 ey = FMath::Clamp(FMath::FloorToInt(mapY), 0, mapHeight - 1);
	const float level = waterLevelField[(size_t)ey * waterFieldWidth + ex];
	if (level <= FTerrainMaterialBuilder::NoWater * 0.5f) return none;
	return (level - height) * FTerrainMaterialBuilder::MetersPerElement;
}

// 隧道口侧壁：对每个wall=true的hatch(RoadnetMod::AddTunnelHatch)，在洞的两条长边(沿rotation，横向
// 偏移+-sizeY/2)各做一条竖直的墙；不加两个端头(起点端是和地面齐平的路面入口，终点端是隧道本身，隧道里
// 的墙和顶棚用资产)。墙顶=不低于所在地形(取附近内外两侧地形最高处，可略突出地面，不留空缺)，墙底=洞底(floorStart->floorEnd，smoothstep)再往下埋一点；做成有
// 厚度的小板(朝洞内的内面+朝洞外的外面+顶面)，内面可见、可碰撞。其它hatch(wall=false)一概没有墙。
#define TUNNEL_WALL_SEGMENT 0.25f       // 沿长度的断面间距(地图element单位)，墙顶跟着地形起伏
#define TUNNEL_WALL_THICKNESS 0.1f      // 墙厚度
#define TUNNEL_WALL_BOTTOM_EXTRA 0.1f   // 墙底比洞底再往下埋多少(埋在路面下)
#define TUNNEL_WALL_TOP_MARGIN 0.005f   // 侧墙/端墙顶比所在地形再高多少(0.005地图单位=5cm)，保证稳定凸出地形

void UForeverTerrainFrameworkComponent::BuildTunnelWalls() {
	if (!map) return;
	AActor* owner = GetOwner();
	if (!owner) return;

	const float worldScale = 1000.f;
	TArray<FVector> vertices;
	TArray<int32> triangles;
	TArray<FVector> normals;

	// 按法线方向定三角形绕序：本文件约定(A,B,C)的朝向是Cross(C-A,B-A)(和BuildLevel算面法线一致)，
	// 朝向和期望法线相反就交换B/C。每个三角形独立顶点(平直着色)。
	auto addTriangle = [&](FVector a, FVector b, FVector c, const FVector& normal) {
		if (FVector::DotProduct(FVector::CrossProduct(c - a, b - a), normal) < 0.f) Swap(b, c);
		int32 base = vertices.Num();
		vertices.Add(a); vertices.Add(b); vertices.Add(c);
		normals.Add(normal); normals.Add(normal); normals.Add(normal);
		triangles.Add(base); triangles.Add(base + 1); triangles.Add(base + 2);
		};
	auto addQuad = [&](const FVector& a, const FVector& b, const FVector& c, const FVector& d, const FVector& normal) {
		addTriangle(a, b, c, normal);
		addTriangle(a, c, d, normal);
		};

	for (const TerrainHatch& hatch : map->GetTunnelWallHatches()) {
		float length = hatch.quad.GetSizeX();
		float halfWidth = hatch.quad.GetSizeY() * 0.5f;
		if (length <= 0.f || halfWidth <= 0.f) continue;
		float cosRot = FMath::Cos(hatch.rotation), sinRot = FMath::Sin(hatch.rotation);
		FVector2D forward(cosRot, sinRot), perp(-sinRot, cosRot);
		FVector2D center(hatch.quad.GetPosX(), hatch.quad.GetPosY());

		// 洞底高度沿路是起点端到终点端的smoothstep(隧道口下坡的S形曲线两端水平)。
		auto floorAt = [&](float u) {
			float s = u * u * (3.f - 2.f * u);
			return hatch.floorStart + (hatch.floorEnd - hatch.floorStart) * s;
			};

		int segments = FMath::Max(1, FMath::CeilToInt(length / TUNNEL_WALL_SEGMENT));
		for (int side = -1; side <= 1; side += 2) {
			// 墙面法线(水平)：内面朝洞中心线，外面朝洞外。
			FVector inward(-perp.X * side, -perp.Y * side, 0.f);
			FVector outward = -inward;
			for (int i = 0; i < segments; i++) {
				float u0 = static_cast<float>(i) / segments, u1 = static_cast<float>(i + 1) / segments;
				FVector2D p0 = center + forward * ((u0 - 0.5f) * length) + perp * (side * halfWidth);
				FVector2D p1 = center + forward * ((u1 - 0.5f) * length) + perp * (side * halfWidth);
				float floor0 = floorAt(u0), floor1 = floorAt(u1);
				FVector2D o0 = p0 + perp * (side * TUNNEL_WALL_THICKNESS);
				FVector2D o1 = p1 + perp * (side * TUNNEL_WALL_THICKNESS);
				// 墙顶不低于所在地形(和两端的端墙一致)：墙顶点取附近一小段(前后各半个断面间距)里内外两侧地形高度的最大值，
				// 可以略突出地面，但不留空缺；内外两侧同高，顶面是平的。
				auto topAt = [&](float u) {
					float best = -FLT_MAX;
					for (float du : { -0.5f, 0.f, 0.5f }) {
						float uu = FMath::Clamp(u + du / segments, 0.f, 1.f);
						FVector2D q = center + forward * ((uu - 0.5f) * length) + perp * (side * halfWidth);
						FVector2D qo = q + perp * (side * TUNNEL_WALL_THICKNESS);
						best = FMath::Max(best, FMath::Max(SampleHeight(q.X, q.Y), SampleHeight(qo.X, qo.Y)));
					}
					return best;
				};
				float top0 = topAt(u0), top1 = topAt(u1);
				// 这一段地形不比洞底高(洞底和地面齐平的入口附近)就不需要墙。
				if (top0 <= floor0 && top1 <= floor1) continue;
				// 再留1cm余量，墙顶稳定凸出地形(地形网格是三角面片，SampleHeight取最大值仍可能被插值误差反超)。
				top0 += TUNNEL_WALL_TOP_MARGIN; top1 += TUNNEL_WALL_TOP_MARGIN;
				float outerTop0 = top0, outerTop1 = top1;
				float bottom0 = floor0 - TUNNEL_WALL_BOTTOM_EXTRA, bottom1 = floor1 - TUNNEL_WALL_BOTTOM_EXTRA;
				auto world = [&](const FVector2D& p, float z) { return FVector(p.X * worldScale, p.Y * worldScale, z * worldScale); };

				addQuad(world(p0, bottom0), world(p1, bottom1), world(p1, top1), world(p0, top0), inward);
				addQuad(world(o0, bottom0), world(o1, bottom1), world(o1, outerTop1), world(o0, outerTop0), outward);
				addQuad(world(p0, top0), world(p1, top1), world(o1, outerTop1), world(o0, outerTop0), FVector::UpVector);
			}
		}

		// 端墙：洞的t1端/t2端各自可选，封住隧道资产房顶和地形之间的空洞。墙是一块矩形板，横向盖满洞宽(两端再各多一个
		// 厚度，和侧壁外侧板接上)，沿长度方向从端头线向洞外厚TUNNEL_WALL_THICKNESS；墙底=指定高度，墙顶=端头线段
		// 范围内地形的最高处(山坡上取最高点，墙顶不降inset，允许略突出地面但不留缺口)。
		auto addEndWall = [&](float endU, float bottom) {
			const float outwardSign = endU > 0.5f ? 1.f : -1.f;
			FVector2D endCenter = center + forward * ((endU - 0.5f) * length);
			FVector2D outwardDir = forward * outwardSign;
			float lateral = halfWidth + TUNNEL_WALL_THICKNESS;
			int samples = FMath::Max(1, FMath::CeilToInt(2.f * lateral / TUNNEL_WALL_SEGMENT));
			float top = -FLT_MAX;
			for (int i = 0; i <= samples; i++) {
				float v = -lateral + 2.f * lateral * static_cast<float>(i) / samples;
				FVector2D base = endCenter + perp * v;
				FVector2D outer = base + outwardDir * TUNNEL_WALL_THICKNESS;
				top = FMath::Max(top, FMath::Max(SampleHeight(base.X, base.Y), SampleHeight(outer.X, outer.Y)));
			}
			top += TUNNEL_WALL_TOP_MARGIN; // 端墙顶也留余量，稳定凸出地形
			if (top <= bottom) return;
			FVector2D a = endCenter - perp * lateral, b = endCenter + perp * lateral;
			FVector2D ao = a + outwardDir * TUNNEL_WALL_THICKNESS, bo = b + outwardDir * TUNNEL_WALL_THICKNESS;
			auto world = [&](const FVector2D& p, float z) { return FVector(p.X * worldScale, p.Y * worldScale, z * worldScale); };
			FVector outward(outwardDir.X, outwardDir.Y, 0.f);
			addQuad(world(a, bottom), world(b, bottom), world(b, top), world(a, top), -outward); // 内面：朝洞内
			addQuad(world(ao, bottom), world(bo, bottom), world(bo, top), world(ao, top), outward);
			addQuad(world(a, top), world(b, top), world(bo, top), world(ao, top), FVector::UpVector);
			// 补全长方体的其余三面：墙顶可能突出地面，两个侧面和底面都可能露出来，不能省。
			const FVector perpDir(perp.X, perp.Y, 0.f);
			addQuad(world(a, bottom), world(ao, bottom), world(ao, top), world(a, top), -perpDir);
			addQuad(world(b, bottom), world(bo, bottom), world(bo, top), world(b, top), perpDir);
			addQuad(world(a, bottom), world(b, bottom), world(bo, bottom), world(ao, bottom), -FVector::UpVector);
		};
		if (hatch.startWall) addEndWall(0.f, hatch.startWallBottom);
		if (hatch.endWall) addEndWall(1.f, hatch.endWallBottom);
	}

	if (!tunnelWallMesh) {
		tunnelWallMesh = NewObject<UProceduralMeshComponent>(owner, TEXT("TunnelWallMesh"));
		tunnelWallMesh->SetupAttachment(owner->GetRootComponent());
		tunnelWallMesh->RegisterComponent();
		owner->AddInstanceComponent(tunnelWallMesh);
	}
	if (triangles.Num() == 0) {
		tunnelWallMesh->ClearAllMeshSections();
		return;
	}

	tunnelWallMesh->CreateMeshSection(0, vertices, triangles, normals,
		TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
	if (!tunnelWallMaterial && tunnelWallBaseMaterial) {
		tunnelWallMaterial = UMaterialInstanceDynamic::Create(tunnelWallBaseMaterial, this);
		tunnelWallMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.32f, 0.30f, 0.27f, 1.f));
	}
	if (tunnelWallMaterial) tunnelWallMesh->SetMaterial(0, tunnelWallMaterial);
}
