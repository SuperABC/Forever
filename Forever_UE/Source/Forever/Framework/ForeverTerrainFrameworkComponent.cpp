#include "Framework/ForeverTerrainFrameworkComponent.h"

#include "ProceduralMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

#include "map/map.h"
#include "map/geometry.h"

#define DIFFUSE_TEX_SIZE 2048
#define HEIGHT_EPSILON 0.002f

using namespace std;

UForeverTerrainFrameworkComponent::UForeverTerrainFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> terrainMatFinder(
		TEXT("/Game/Asset/Materials/TemplateTerrain.TemplateTerrain"));
	if (terrainMatFinder.Succeeded()) {
		terrainBaseMaterial = terrainMatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> fineMatFinder(
		TEXT("/Game/Asset/Materials/TemplateFine.TemplateFine"));
	if (fineMatFinder.Succeeded()) {
		fineBaseMaterial = fineMatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> oceanMatFinder(
		TEXT("/Water/Materials/WaterSurface/Water_Material_CustomMesh.Water_Material_CustomMesh"));
	if (oceanMatFinder.Succeeded()) {
		oceanBaseMaterial = oceanMatFinder.Object;
	}
}

void UForeverTerrainFrameworkComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!map) return;
	if (currentPivots.empty()) return;

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
			parentOffsetX = idxX * 8;
			parentOffsetY = idxY * 8;
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

	const auto& terrainTextures = map->GetTerrainTextures();
	if (!terrainTextures.empty()) {
		int32 numSlices = static_cast<int32>(terrainTextures.size());

		TArray<UTexture2D*> sourceTextures;
		sourceTextures.SetNum(numSlices);
		for (auto& [textureName, textureInfo] : terrainTextures)
			if (sourceTextures.IsValidIndex(textureInfo.first) && !textureInfo.second.empty())
				sourceTextures[textureInfo.first] = LoadObject<UTexture2D>(nullptr, *FString(textureInfo.second.c_str()));

		// 构建Texture2DArray
		terrainDiffuseArray = NewObject<UTexture2DArray>(this, NAME_None, RF_Transient);
		terrainDiffuseArray->Filter = TF_Bilinear;
		terrainDiffuseArray->SRGB = true;
#if WITH_EDITOR
		terrainDiffuseArray->MipGenSettings = TMGS_NoMipmaps;
#endif
		terrainDiffuseArray->NeverStream = true;

		FTexturePlatformData* platformData = new FTexturePlatformData();
		platformData->SizeX = DIFFUSE_TEX_SIZE;
		platformData->SizeY = DIFFUSE_TEX_SIZE;
		platformData->SetNumSlices(numSlices);
		platformData->PixelFormat = PF_B8G8R8A8;

		FTexture2DMipMap* mip = new FTexture2DMipMap();
		mip->SizeX = DIFFUSE_TEX_SIZE;
		mip->SizeY = DIFFUSE_TEX_SIZE;
		mip->SizeZ = numSlices;
		int32 sliceBytes = DIFFUSE_TEX_SIZE * DIFFUSE_TEX_SIZE * 4;

		mip->BulkData.Lock(LOCK_READ_WRITE);
		uint8* destPixels = (uint8*)mip->BulkData.Realloc((int64)sliceBytes * numSlices);
		FMemory::Memzero(destPixels, (int64)sliceBytes * numSlices);

		for (int32 sliceIndex = 0; sliceIndex < sourceTextures.Num(); sliceIndex++) {
			UTexture2D* srcTexture = sourceTextures[sliceIndex];
			if (!srcTexture) continue;
#if WITH_EDITOR
			TArray64<uint8> srcBytes;
			if (srcTexture->Source.GetMipData(srcBytes, 0) && srcBytes.Num() >= sliceBytes)
				FMemory::Memcpy(destPixels + (int64)sliceIndex * sliceBytes, srcBytes.GetData(), sliceBytes);
#else
			FTexturePlatformData* srcPlatformData = srcTexture->GetPlatformData();
			if (srcPlatformData && srcPlatformData->Mips.Num() > 0) {
				FByteBulkData& bulkData = srcPlatformData->Mips[0].BulkData;
				const void* srcData = bulkData.Lock(LOCK_READ_ONLY);
				if (srcData && bulkData.GetBulkDataSize() >= sliceBytes)
					FMemory::Memcpy(destPixels + (int64)sliceIndex * sliceBytes, srcData, sliceBytes);
				bulkData.Unlock();
			}
#endif
		}

		mip->BulkData.Unlock();
		platformData->Mips.Add(mip);
		terrainDiffuseArray->SetPlatformData(platformData);
		terrainDiffuseArray->UpdateResource();

		// 构建地形索引图:每像素存储该元素的地形类型槽位索引(PF_G8)
		auto [mapWidth, mapHeight] = map->GetSize();
		terrainIndexMap = UTexture2D::CreateTransient(mapWidth, mapHeight, PF_G8, TEXT("TerrainIndexMap"));
		terrainIndexMap->NeverStream = true;
		terrainIndexMap->Filter = TF_Nearest;

		FTexture2DMipMap& indexMip = terrainIndexMap->GetPlatformData()->Mips[0];
		uint8* indexPixels = (uint8*)indexMip.BulkData.Lock(LOCK_READ_WRITE);
		for (int y = 0; y < mapHeight; y++)
			for (int x = 0; x < mapWidth; x++) {
				const string& terrainName = map->GetTerrain(x, y);
				uint8 terrainIndex = 0;
				auto terrainIter = terrainTextures.find(terrainName);
				if (terrainIter != terrainTextures.end())
					terrainIndex = (uint8)FMath::Min(terrainIter->second.first, 255);
				indexPixels[y * mapWidth + x] = terrainIndex;
			}
		indexMip.BulkData.Unlock();
		terrainIndexMap->UpdateResource();

		if (terrainBaseMaterial) {
			terrainMaterial = UMaterialInstanceDynamic::Create(terrainBaseMaterial, this);
			terrainMaterial->SetTextureParameterValue(TEXT("TerrainDiffuseArray"), terrainDiffuseArray);
			terrainMaterial->SetTextureParameterValue(TEXT("TerrainIndexMap"), terrainIndexMap);
		}

		if (fineBaseMaterial) {
			fineMaterial0 = UMaterialInstanceDynamic::Create(fineBaseMaterial, this);
			fineMaterial1 = UMaterialInstanceDynamic::Create(fineBaseMaterial, this);
			fineMaterial2 = UMaterialInstanceDynamic::Create(fineBaseMaterial, this);
		}
	}

	gridMeshes.SetNum(num);
	for (int i = 0; i < num; i++) {
		UProceduralMeshComponent* comp = NewObject<UProceduralMeshComponent>(
			owner, *FString::Printf(TEXT("TerrainLod_%d"), i));
		comp->SetupAttachment(owner->GetRootComponent());
		comp->RegisterComponent();
		owner->AddInstanceComponent(comp);
		gridMeshes[i] = comp;
	}

	lodBoundaries.resize(num);
	for (auto& boundary : lodBoundaries) {
		boundary.bottom.SetNum(33);
		boundary.top.SetNum(33);
		boundary.left.SetNum(33);
		boundary.right.SetNum(33);
	}

	BuildOceanMesh();
}

float UForeverTerrainFrameworkComponent::SampleHeight(float mapX, float mapY) const {
	if (!map) return 0.f;
	auto mapSize = map->GetSize();

	int ex0 = FMath::Clamp(FMath::FloorToInt(mapX), 0, mapSize.first - 1);
	int ey0 = FMath::Clamp(FMath::FloorToInt(mapY), 0, mapSize.second - 1);
	int ex1 = FMath::Clamp(ex0 + 1, 0, mapSize.first - 1);
	int ey1 = FMath::Clamp(ey0 + 1, 0, mapSize.second - 1);
	float tx = mapX - FMath::FloorToInt(mapX);
	float ty = mapY - FMath::FloorToInt(mapY);

	return map->GetHeight(ex0, ey0) * (1 - tx) * (1 - ty) +
		map->GetHeight(ex1, ey0) * tx * (1 - ty) +
		map->GetHeight(ex0, ey1) * (1 - tx) * ty +
		map->GetHeight(ex1, ey1) * tx * ty;
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
	// 用有向线段(edgeA->edgeB)所在直线，把一个凸多边形(polygon，任意绕序，但假定和
	// referencePoint的绕序一致——referencePoint是已知严格落在多边形"内部半平面"那一侧的点，
	// 不会正好落在这条直线上)分成两部分：outInside(和referencePoint同一侧)/outOutside(不同
	// 侧)，标准Sutherland-Hodgman单边裁剪算法的双输出版本——outInside/outOutside都保持
	// 原polygon的绕序不变(交点按遍历顺序插入)，可以直接继续参与下一条边的裁剪或直接扇形三角化。
	void SplitConvexPolygonByLine(const TArray<FVector2D>& polygon, const FVector2D& edgeA, const FVector2D& edgeB,
		const FVector2D& referencePoint, TArray<FVector2D>& outInside, TArray<FVector2D>& outOutside) {
		outInside.Reset();
		outOutside.Reset();
		int32 n = polygon.Num();
		if (n < 3) return;

		FVector2D dir = edgeB - edgeA;
		auto side = [&](const FVector2D& p) { return dir.X * (p.Y - edgeA.Y) - dir.Y * (p.X - edgeA.X); };
		bool refPositive = side(referencePoint) >= 0.f;

		for (int32 i = 0; i < n; i++) {
			const FVector2D& cur = polygon[i];
			const FVector2D& next = polygon[(i + 1) % n];
			float curSide = side(cur);
			float nextSide = side(next);
			bool curInside = (curSide >= 0.f) == refPositive;
			bool nextInside = (nextSide >= 0.f) == refPositive;

			if (curInside) outInside.Add(cur); else outOutside.Add(cur);

			if (curInside != nextInside) {
				float t = curSide / (curSide - nextSide);
				FVector2D intersection = cur + (next - cur) * t;
				outInside.Add(intersection);
				outOutside.Add(intersection);
			}
		}
	}

	// 把一个凸多边形(CCW，同本文件主网格quad(v00,v11,v10)/(v00,v01,v11)那一套已验证过的绕序
	// 约定：对(左下,右下,右上,左上)这种CCW四边形要输出(v0,v2,v1)+(v0,v3,v2)而不是朴素的
	// (v0,v1,v2)+(v0,v2,v3)才能让法线朝上)扇形三角化成FTerrainTri2D列表——把朴素fan的每个
	// 三角形后两个顶点对调，泛化到任意点数的凸多边形。
	void FanTriangulate(const TArray<FVector2D>& polygon, TArray<FTerrainTri2D>& outTris) {
		for (int32 i = 1; i + 1 < polygon.Num(); i++) {
			outTris.Add({ polygon[0], polygon[i + 1], polygon[i] });
		}
	}
}

void UForeverTerrainFrameworkComponent::LookupTerrain(int elemX, int elemY, FString& type, float& height,
	TArray<FTerrainTri2D>& tris) const {
	if (!map) return;

	type = FString(map->GetTerrain(elemX, elemY).c_str());
	// 挖洞不再只限"construction"格子——Roadnet阶段-2给隧道口调用Map::AddHatch时，落点的格子
	// 地形类型是"mountain"（或紧邻的"plain"），只要这个格子有hatch就要继续走多边形裁剪逻辑，
	// 不能在这里直接退出，否则隧道段会被山体实心地形完全挡住看不见，详见roadnet_basic.md
	// "隧道"一节。
	if (type != "construction" && map->GetHatches(elemX, elemY).empty()) return;

	height = map->GetHeight(elemX, elemY);
	tris.Empty();

	// 这个格子里"还没被判定为实心/还没被判定为洞、需要继续跟下一个hatch比对"的剩余区域，可能
	// 不止一块(多边形列表)，局部坐标[0,1]x[0,1]，初始就是整个格子(单一多边形,CCW:
	// 左下->右下->右上->左上，和本文件主网格quad同一套绕序约定)。
	TArray<TArray<FVector2D>> workPolys;
	workPolys.Add({ {0.f,0.f}, {1.f,0.f}, {1.f,1.f}, {0.f,1.f} });

	for (auto& [quad, rotation] : map->GetHatches(elemX, elemY)) {
		if (workPolys.Num() == 0) break; // 这个格子已经被之前的hatch挖空了，不用再判断了

		float hatchCenterX = quad.GetPosX() - elemX;
		float hatchCenterY = quad.GetPosY() - elemY;
		float halfSizeX = quad.GetSizeX() * 0.5f;
		float halfSizeY = quad.GetSizeY() * 0.5f;
		float cosRot = FMath::Cos(rotation), sinRot = FMath::Sin(rotation);

		// 旋转矩形AABB快速剔除：这个hatch的外接矩形都不沾这个格子的边，直接跳过，不用做4次
		// 多边形裁剪。
		float aabbHalfX = FMath::Abs(halfSizeX * cosRot) + FMath::Abs(halfSizeY * sinRot);
		float aabbHalfY = FMath::Abs(halfSizeX * sinRot) + FMath::Abs(halfSizeY * cosRot);
		if (hatchCenterX + aabbHalfX <= 0.f || hatchCenterX - aabbHalfX >= 1.f) continue;
		if (hatchCenterY + aabbHalfY <= 0.f || hatchCenterY - aabbHalfY >= 1.f) continue;

		// hatch矩形自己的4个顶点(局部坐标)——绕序(顺时针还是逆时针不重要，下面裁剪用
		// "和hatchCenter同一侧"判断，不依赖绝对绕序方向)。
		FVector2D hatchCenter(hatchCenterX, hatchCenterY);
		FVector2D hatchPoly[4] = {
			{ hatchCenterX + halfSizeX * cosRot - halfSizeY * sinRot, hatchCenterY + halfSizeX * sinRot + halfSizeY * cosRot },
			{ hatchCenterX - halfSizeX * cosRot - halfSizeY * sinRot, hatchCenterY - halfSizeX * sinRot + halfSizeY * cosRot },
			{ hatchCenterX - halfSizeX * cosRot + halfSizeY * sinRot, hatchCenterY - halfSizeX * sinRot - halfSizeY * cosRot },
			{ hatchCenterX + halfSizeX * cosRot + halfSizeY * sinRot, hatchCenterY + halfSizeX * sinRot - halfSizeY * cosRot },
		};

		// 依次按hatch矩形的4条边裁剪：每一块现有的剩余区域，只要在某一条边的"外侧"，就已经能
		// 确定它落在hatch矩形整体的外面(4个半平面的交集之外)——不用再继续判断剩下的边，直接
		// 进survivingPieces，留到下一个hatch接着比对；"内侧"的部分还要继续拿下一条边判断。
		// 4条边全部判断完之后还留在currentInside里的，就是真正同时落在4个半平面内部、被这个
		// hatch矩形真正挖穿的洞——直接丢弃，不进入survivingPieces。这个"依次按半平面分割、
		// 外侧确定即收下"的结构和原来按AABB做T形分割是同一个思路，只是把"轴对齐矩形的4条边"
		// 换成"任意旋转矩形的4条边"，因此不再需要"只在包含hatch中心的格子里补角落三角形"这个
		// 只对"整个hatch都落在单一格子内"才成立的特例——不管hatch跨了几个格子，每个格子都独立
		// 按自己的[0,1]范围和hatch的4条边精确裁剪，天然得到正确结果。
		TArray<TArray<FVector2D>> survivingPieces;
		for (const TArray<FVector2D>& piece : workPolys) {
			TArray<FVector2D> currentInside = piece;
			for (int32 e = 0; e < 4 && currentInside.Num() >= 3; e++) {
				TArray<FVector2D> insidePart, outsidePart;
				SplitConvexPolygonByLine(currentInside, hatchPoly[e], hatchPoly[(e + 1) % 4], hatchCenter, insidePart, outsidePart);
				if (outsidePart.Num() >= 3) survivingPieces.Add(outsidePart);
				currentInside = MoveTemp(insidePart);
			}
		}
		workPolys = MoveTemp(survivingPieces);
	}

	// workPolys现在就是这个格子里真正的实心地形区域(可能是好几块互不相连的多边形)，扇形三角化
	// 成最终输出——没有任何hatch命中时workPolys就是初始的整格方块，输出2个三角形，和原来
	// "没有hatch命中时rects恒为整格一个矩形"的行为完全一致。
	for (const TArray<FVector2D>& piece : workPolys) {
		FanTriangulate(piece, tris);
	}
}

void UForeverTerrainFrameworkComponent::BuildLevel(int levelIdx, pair<int, int> pos, float size, int childPos,
	const FForeverLodBoundary* parentBoundary, int parentOffsetX, int parentOffsetY, int edgeFlags) {
	if (!map) return;

	auto mapSize = map->GetSize();
	UProceduralMeshComponent* mesh = gridMeshes[levelIdx];

	const float cellSize = size / 32.f;
	const float startX = pos.first * (size / 2.f) - size / 2.f;
	const float startY = pos.second * (size / 2.f) - size / 2.f;
	const float worldScale = 1000.f;

	TArray<FVector> vertices;
	TArray<int32> triangles;
	TArray<FVector2D> uvs;

	vertices.Reserve(33 * 33);
	uvs.Reserve(33 * 33);

	for (int vy = 0; vy <= 32; vy++) {
		for (int vx = 0; vx <= 32; vx++) {
			float mapX = startX + vx * cellSize;
			float mapY = startY + vy * cellSize;
			float h = SampleHeight(mapX, mapY) + HEIGHT_EPSILON;

			vertices.Add(FVector(mapX * worldScale, mapY * worldScale, h * worldScale));
			if (levelIdx <= 2)
				uvs.Add(FVector2D(vx / 32.f, vy / 32.f));
			else
				uvs.Add(FVector2D(mapX, mapY));
		}
	}

	auto vertIdx = [](int vx, int vy) { return vy * 33 + vx; };

	// 若与父级(粗一级)LOD存在共享边,先将偶数顶点对齐到父级已修正的边界值,
	// 再统一做奇数插值,确保多级边界完全一致
	if (parentBoundary && edgeFlags) {
		if (edgeFlags & 0x1) // 左边(vx=0)与父级左边重合
			for (int k = 0; k <= 32; k += 2)
				vertices[vertIdx(0, k)] = parentBoundary->left[parentOffsetY + k / 2];
		if (edgeFlags & 0x2) // 右边(vx=32)与父级右边重合
			for (int k = 0; k <= 32; k += 2)
				vertices[vertIdx(32, k)] = parentBoundary->right[parentOffsetY + k / 2];
		if (edgeFlags & 0x4) // 下边(vy=0)与父级下边重合
			for (int k = 0; k <= 32; k += 2)
				vertices[vertIdx(k, 0)] = parentBoundary->bottom[parentOffsetX + k / 2];
		if (edgeFlags & 0x8) // 上边(vy=32)与父级上边重合
			for (int k = 0; k <= 32; k += 2)
				vertices[vertIdx(k, 32)] = parentBoundary->top[parentOffsetX + k / 2];
	}

	// 让边缘在粗一级LOD的格点间距上保持分段线性,消除接缝裂缝
	for (int edgeVy : { 0, 32 })
		for (int vx = 1; vx < 32; vx += 2)
			vertices[vertIdx(vx, edgeVy)] = (vertices[vertIdx(vx - 1, edgeVy)] + vertices[vertIdx(vx + 1, edgeVy)]) * 0.5f;
	for (int edgeVx : { 0, 32 })
		for (int vy = 1; vy < 32; vy += 2)
			vertices[vertIdx(edgeVx, vy)] = (vertices[vertIdx(edgeVx, vy - 1)] + vertices[vertIdx(edgeVx, vy + 1)]) * 0.5f;

	// construction/挖洞格子(取代旧工程的ISM立方体):只在levelIdx<=1(近处精细LOD)生效,和旧工程
	// constructionRegion的作用范围一致。前提：所有挖洞都发生在平地上，construction格子不需要
	// 更细的LOD细分——没有hatch命中时LookupTerrain恒返回"整格一个矩形"，直接画成一个大quad
	// (2个三角形)；有hatch命中时返回精确的轴对齐矩形集合(rects)+角落补丁三角形(tris)，两者都
	// 直接按精确坐标建geometry，不再对着一个固定的sub-quad网格做"quad中心是否落在矩形内"的
	// 近似测试——那种测试量出来的洞边界只能精确到sub-quad网格的粒度，和真实矩形边界对不上，
	// 会带出明显的格子锯齿，详见ForeverTerrainFrameworkComponent.md"挖洞"一节这次的修正说明。
	TMap<TPair<int32, int32>, TArray<FTerrainTri2D>> constructionCache;
	auto lookupCached = [this, &constructionCache](int ex, int ey) -> const TArray<FTerrainTri2D>& {
		TPair<int32, int32> key(ex, ey);
		if (auto* found = constructionCache.Find(key)) return *found;
		FString type; float h = 0.f;
		TArray<FTerrainTri2D> tris;
		LookupTerrain(ex, ey, type, h, tris);
		return constructionCache.Add(key, MoveTemp(tris));
		};

	for (int cy = 0; cy < 32; cy++) {
		for (int cx = 0; cx < 32; cx++) {
			int s = (cy / 8) * 4 + (cx / 8);
			if ((childPos >> (15 - s)) & 1) continue;

			bool drawQuad = true;
			if (levelIdx <= 1) {
				float quadCenterMapX = startX + (cx + 0.5f) * cellSize;
				float quadCenterMapY = startY + (cy + 0.5f) * cellSize;
				int ex = FMath::Clamp(FMath::FloorToInt(quadCenterMapX), 0, mapSize.first - 1);
				int ey = FMath::Clamp(FMath::FloorToInt(quadCenterMapY), 0, mapSize.second - 1);
				// "construction"格子或者被Map::AddHatch记过洞(目前只有Roadnet隧道口这一个来源)
				// 的格子，整格都交给下面精确geometry那一段处理，这里只负责登记(触发lookupCached
				// 缓存)+跳过常规两三角形画法，不再做任何"quad中心是否在矩形内"的判断。
				if (map->GetTerrain(ex, ey) == "construction" || !map->GetHatches(ex, ey).empty()) {
					lookupCached(ex, ey);
					drawQuad = false;
				}
			}
			if (!drawQuad) continue;

			int32 v00 = cy * 33 + cx;
			int32 v10 = cy * 33 + cx + 1;
			int32 v01 = (cy + 1) * 33 + cx;
			int32 v11 = (cy + 1) * 33 + cx + 1;
			triangles.Add(v00); triangles.Add(v11); triangles.Add(v10);
			triangles.Add(v00); triangles.Add(v01); triangles.Add(v11);
		}
	}

	// 为每个涉及到的construction/挖洞Element画出LookupTerrain精确裁剪出的实心地形三角形——
	// 没有hatch命中时就是完整一格的2个三角形，有hatch命中(含跨格)时是裁剪出的精确形状，绕序
	// (A,B,C)已经在LookupTerrain的FanTriangulate里按本函数主网格quad同一套约定处理过，这里
	// 直接按原顺序建三角形，不用再对调。
	if (levelIdx <= 1) {
		for (const auto& entry : constructionCache) {
			int32 ex = entry.Key.Key, ey = entry.Key.Value;

			for (const FTerrainTri2D& tri : entry.Value) {
				FVector2D corners[3] = { tri.A, tri.B, tri.C };
				int32 baseIdx = vertices.Num();
				for (const FVector2D& corner : corners) {
					float mapX = ex + corner.X;
					float mapY = ey + corner.Y;
					float h = SampleHeight(mapX, mapY) + HEIGHT_EPSILON;
					vertices.Add(FVector(mapX * worldScale, mapY * worldScale, h * worldScale));
					uvs.Add(FVector2D(mapX, mapY));
				}
				triangles.Add(baseIdx); triangles.Add(baseIdx + 1); triangles.Add(baseIdx + 2);
			}
		}
	}

	mesh->CreateMeshSection(0, vertices, triangles,
		TArray<FVector>(), uvs, TArray<FColor>(), TArray<FProcMeshTangent>(), true);

	// 保存修正后的四条边界顶点,供更细一级LOD进行接缝修正时使用
	if (levelIdx < (int)lodBoundaries.size()) {
		FForeverLodBoundary& boundary = lodBoundaries[levelIdx];
		for (int k = 0; k <= 32; k++) {
			boundary.bottom[k] = vertices[vertIdx(k, 0)];
			boundary.top[k] = vertices[vertIdx(k, 32)];
			boundary.left[k] = vertices[vertIdx(0, k)];
			boundary.right[k] = vertices[vertIdx(32, k)];
		}
	}

	if (levelIdx <= 2) {
		// lod0/1/2的cell比element还小,需要细分地形混合数据
		int32 baseRes = FMath::RoundToInt(size);
		TArray<FFineCell> cells;
		cells.SetNum(baseRes * baseRes);

		int baseX = FMath::FloorToInt(startX);
		int baseY = FMath::FloorToInt(startY);
		const auto& texInfo = map->GetTerrainTextures();
		for (int y = 0; y < baseRes; y++) {
			for (int x = 0; x < baseRes; x++) {
				int ex = FMath::Clamp(baseX + x, 0, mapSize.first - 1);
				int ey = FMath::Clamp(baseY + y, 0, mapSize.second - 1);
				auto it = texInfo.find(map->GetTerrain(ex, ey));
				FFineCell& cell = cells[y * baseRes + x];
				cell.Ids[0] = (it != texInfo.end()) ? (uint8)it->second.first : 0;
				cell.Weights[0] = 1.f;
			}
		}

		int32 curRes = baseRes;
		int32 steps = 3 - levelIdx;
		for (int32 s = 0; s < steps; s++) {
			TArray<FFineCell> next;
			DownsampleFine(cells, curRes, next);
			cells = MoveTemp(next);
			curRes *= 2;
		}

		UTexture2D* fineIdx = UTexture2D::CreateTransient(32, 32, PF_B8G8R8A8);
		UTexture2D* finePow = UTexture2D::CreateTransient(32, 32, PF_B8G8R8A8);
		if (fineIdx && finePow) {
			fineIdx->Filter = TF_Nearest;
			fineIdx->SRGB = false;
			finePow->Filter = TF_Bilinear;
			finePow->SRGB = false;

			FTexturePlatformData* idxPd = fineIdx->GetPlatformData();
			FTexturePlatformData* powPd = finePow->GetPlatformData();
			if (idxPd && idxPd->Mips.Num() > 0 && powPd && powPd->Mips.Num() > 0) {
				TArray<FColor> idxPixels, powPixels;
				idxPixels.SetNum(32 * 32);
				powPixels.SetNum(32 * 32);
				for (int i = 0; i < 32 * 32; i++) {
					const FFineCell& cell = cells[i];
					idxPixels[i] = FColor(cell.Ids[0], cell.Ids[1], cell.Ids[2], cell.Ids[3]);
					powPixels[i] = FColor(
						(uint8)FMath::Clamp(FMath::RoundToInt(cell.Weights[0] * 255.f), 0, 255),
						(uint8)FMath::Clamp(FMath::RoundToInt(cell.Weights[1] * 255.f), 0, 255),
						(uint8)FMath::Clamp(FMath::RoundToInt(cell.Weights[2] * 255.f), 0, 255),
						(uint8)FMath::Clamp(FMath::RoundToInt(cell.Weights[3] * 255.f), 0, 255));
				}

				const int32 bytes = 32 * 32 * 4;
				void* idxData = idxPd->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(idxData, idxPixels.GetData(), bytes);
				idxPd->Mips[0].BulkData.Unlock();

				void* powData = powPd->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(powData, powPixels.GetData(), bytes);
				powPd->Mips[0].BulkData.Unlock();
			}
			fineIdx->UpdateResource();
			finePow->UpdateResource();
		}

		switch (levelIdx) {
		case 0: fineIndexMap0 = fineIdx; finePowerMap0 = finePow; break;
		case 1: fineIndexMap1 = fineIdx; finePowerMap1 = finePow; break;
		case 2: fineIndexMap2 = fineIdx; finePowerMap2 = finePow; break;
		default: break;
		}

		UMaterialInstanceDynamic* levelMat = nullptr;
		switch (levelIdx) {
		case 0: levelMat = fineMaterial0; break;
		case 1: levelMat = fineMaterial1; break;
		case 2: levelMat = fineMaterial2; break;
		default: break;
		}
		if (levelMat) {
			levelMat->SetTextureParameterValue(TEXT("FineIndexMap"), fineIdx);
			levelMat->SetTextureParameterValue(TEXT("FinePowerMap"), finePow);
			levelMat->SetTextureParameterValue(TEXT("TerrainDiffuseArray"), terrainDiffuseArray);
			mesh->SetMaterial(0, levelMat);
		}
	}
	else if (terrainMaterial) {
		mesh->SetMaterial(0, terrainMaterial);
	}
}

void UForeverTerrainFrameworkComponent::DownsampleFine(const TArray<FFineCell>& in, int32 inRes, TArray<FFineCell>& out) const {
	int32 outRes = inRes * 2;
	out.SetNum(outRes * outRes);

	for (int oy = 0; oy < outRes; oy++) {
		for (int ox = 0; ox < outRes; ox++) {
			float fx = (ox + 0.5f) * 0.5f - 0.5f;
			float fy = (oy + 0.5f) * 0.5f - 0.5f;
			int ix0 = FMath::FloorToInt(fx);
			int iy0 = FMath::FloorToInt(fy);
			float tx = fx - ix0;
			float ty = fy - iy0;
			int ix1 = ix0 + 1;
			int iy1 = iy0 + 1;
			ix0 = FMath::Clamp(ix0, 0, inRes - 1);
			ix1 = FMath::Clamp(ix1, 0, inRes - 1);
			iy0 = FMath::Clamp(iy0, 0, inRes - 1);
			iy1 = FMath::Clamp(iy1, 0, inRes - 1);

			const FFineCell* corners[4] = {
				&in[iy0 * inRes + ix0], &in[iy0 * inRes + ix1],
				&in[iy1 * inRes + ix0], &in[iy1 * inRes + ix1]
			};
			float cornerW[4] = { (1 - tx) * (1 - ty), tx * (1 - ty), (1 - tx) * ty, tx * ty };

			TMap<uint8, float> accum;
			for (int c = 0; c < 4; c++) {
				if (cornerW[c] <= 0.f) continue;
				for (int k = 0; k < 4; k++) {
					float w = corners[c]->Weights[k] * cornerW[c];
					if (w <= 0.f) continue;
					accum.FindOrAdd(corners[c]->Ids[k]) += w;
				}
			}

			TArray<TPair<uint8, float>> sorted;
			for (auto& pair : accum) sorted.Add(TPair<uint8, float>(pair.Key, pair.Value));
			sorted.Sort([](const TPair<uint8, float>& a, const TPair<uint8, float>& b) { return a.Value > b.Value; });

			FFineCell cell;
			for (int k = 0; k < 4; k++) {
				if (k < sorted.Num()) { cell.Ids[k] = sorted[k].Key; cell.Weights[k] = sorted[k].Value; }
			}
			out[oy * outRes + ox] = cell;
		}
	}
}

void UForeverTerrainFrameworkComponent::BuildOceanMesh() {
	if (!map) return;

	AActor* owner = GetOwner();
	if (!owner) return;

	auto [width, height] = map->GetSize();
	const float worldScale = 1000.f;

	// 共享网格顶点:(width+1)*(height+1)个格点,高度固定为0
	TArray<FVector> vertices;
	TArray<FVector2D> uvs;
	vertices.Reserve((width + 1) * (height + 1));
	uvs.Reserve((width + 1) * (height + 1));
	for (int vy = 0; vy <= height; vy++)
		for (int vx = 0; vx <= width; vx++) {
			vertices.Add(FVector(vx * worldScale, vy * worldScale, 0.f));
			uvs.Add(FVector2D(vx, vy));
		}

	// 只为地形类型为"ocean"的格子生成两个三角形
	TArray<int32> triangles;
	auto vertIdx = [width](int vx, int vy) { return vy * (width + 1) + vx; };
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			auto [water, h] = map->GetWater(x, y);
			if (!water) continue;

			int idxBL = vertIdx(x, y), idxBR = vertIdx(x + 1, y);
			int idxTL = vertIdx(x, y + 1), idxTR = vertIdx(x + 1, y + 1);
			vertices[idxBL].Z = h * worldScale;
			vertices[idxBR].Z = h * worldScale;
			vertices[idxTL].Z = h * worldScale;
			vertices[idxTR].Z = h * worldScale;
			triangles.Add(idxBL); triangles.Add(idxTL); triangles.Add(idxTR);
			triangles.Add(idxBL); triangles.Add(idxTR); triangles.Add(idxBR);
		}
	}
	if (triangles.Num() == 0) return;

	if (!oceanMesh) {
		oceanMesh = NewObject<UProceduralMeshComponent>(owner, TEXT("OceanMesh"));
		oceanMesh->SetupAttachment(owner->GetRootComponent());
		oceanMesh->RegisterComponent();
		owner->AddInstanceComponent(oceanMesh);
	}

	oceanMesh->CreateMeshSection(0, vertices, triangles,
		TArray<FVector>(), uvs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);

	if (!oceanMaterial && oceanBaseMaterial) {
		oceanMaterial = UMaterialInstanceDynamic::Create(oceanBaseMaterial, this);
	}
	if (oceanMaterial)
		oceanMesh->SetMaterial(0, oceanMaterial);
}
