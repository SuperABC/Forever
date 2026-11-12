#include "Framework/ForeverTerrainFrameworkComponent.h"

#include "ProceduralMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/ForeverFrameworkActor.h"
#include "UObject/ConstructorHelpers.h"

#include "map/map.h"
#include "map/general_geometry.h"

#define DIFFUSE_TEX_SIZE 2048
#define HEIGHT_EPSILON 0.002f
// 每级LOD mesh的网格细分数(每级都是这个数,级与级之间只有size不一样；64即65x65个顶点，128实测太卡)。
// BuildLevel里所有跟它绑定的数(顶点数、行跨度、父子边界偏移)都由它推导，调这一个数即可。
#define TERRAIN_LOD_GRID_SUBDIV 64
// 显示hatch(挖洞)的最远LOD级别(含)：L0~L3的网格小格边长是1/16、1/8、1/4、1/2 element，都不跨element，
// 可以逐格对三角形做hatch裁剪；construction格子的精确几何仍然只在L0/L1做。
#define TERRAIN_HATCH_MAX_LEVEL 3
// L0覆盖4个element、64段，每个element正好16段=TERRAIN_FINE_SUBDIV，各级LOD顶点都落在细化高度
// 网格点上，Map::SampleHeight直接查表；改了这个数也不会出错，只是L0顶点落到网格点之间，改为插值。

using namespace std;

UForeverTerrainFrameworkComponent::UForeverTerrainFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> terrainMatFinder(
		TEXT("/Game/Asset/Terrain/Template/TemplateFar.TemplateFar"));
	if (terrainMatFinder.Succeeded()) {
		terrainBaseMaterial = terrainMatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> nearMatFinder(
		TEXT("/Game/Asset/Terrain/Template/TemplateNear.TemplateNear"));
	if (nearMatFinder.Succeeded()) {
		nearBaseMaterial = nearMatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> tunnelWallMatFinder(
		TEXT("/Game/Asset/Common/Material/PureColor.PureColor"));
	if (tunnelWallMatFinder.Succeeded()) {
		tunnelWallBaseMaterial = tunnelWallMatFinder.Object;
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

		// 只在nearControlLevel自己的pivot变化时重建近处混合控制纹理——粗级pivot变化时所有
		// 更细的级必然也要重建(上面boundaryChanged的传播已经保证needsRebuild[i]对更细级为真)，
		// 所以纹理和网格总是同一帧一起更新，不会出现"网格已经移动但控制纹理还没跟上"的一帧。
		if (i == nearControlLevel) {
			float regionSize = currentPivots[i].second;
			float originX = newPivots[i].first * (regionSize / 2.f) - regionSize / 2.f;
			float originY = newPivots[i].second * (regionSize / 2.f) - regionSize / 2.f;
			RebuildNearControl(originX, originY, FMath::RoundToInt(regionSize));
		}

		const FForeverLodBoundary* parentBoundary = (i < numLevels - 1) ? &lodBoundaries[i + 1] : nullptr;
		BuildLevel(i, newPivots[i], currentPivots[i].second, childPos,
			parentBoundary, parentOffsetX, parentOffsetY, edgeFlags);
		currentPivots[i].first = newPivots[i];
	}
}

namespace {
	// 读取srcTexture的原始像素，按源分辨率最近邻重采样到expectedSize x expectedSize(BGRA8逐像素拷贝)，
	// 源贴图比目标大/小/非正方形都能处理，只有读不到源数据(纹理空/没有mip)才失败。diffuse/normal/roughness数组共用。
	bool LoadRawBGRA8(UTexture2D* srcTexture, int32 expectedSize, TArray64<uint8>& outBytes) {
		if (!srcTexture) return false;
		int64 expectedBytes = (int64)expectedSize * expectedSize * 4;

		TArray64<uint8> rawBytes;
		int32 srcW = 0, srcH = 0;
#if WITH_EDITOR
		srcW = srcTexture->Source.GetSizeX();
		srcH = srcTexture->Source.GetSizeY();
		if (srcW <= 0 || srcH <= 0 || !srcTexture->Source.GetMipData(rawBytes, 0)) return false;
		int64 rawExpected = (int64)srcW * srcH * 4;
		if (rawBytes.Num() < rawExpected) return false;
#else
		FTexturePlatformData* srcPlatformData = srcTexture->GetPlatformData();
		if (!srcPlatformData || srcPlatformData->Mips.Num() == 0) return false;
		srcW = srcPlatformData->Mips[0].SizeX;
		srcH = srcPlatformData->Mips[0].SizeY;
		FByteBulkData& bulkData = srcPlatformData->Mips[0].BulkData;
		const void* srcData = bulkData.Lock(LOCK_READ_ONLY);
		int64 rawExpected = (int64)srcW * srcH * 4;
		bool ok = srcData && srcW > 0 && srcH > 0 && bulkData.GetBulkDataSize() >= rawExpected;
		if (ok) {
			rawBytes.SetNumUninitialized(rawExpected);
			FMemory::Memcpy(rawBytes.GetData(), srcData, rawExpected);
		}
		bulkData.Unlock();
		if (!ok) return false;
#endif

		if (srcW == expectedSize && srcH == expectedSize) {
			outBytes = MoveTemp(rawBytes);
			outBytes.SetNum(expectedBytes, EAllowShrinking::No);
			return true;
		}

		outBytes.SetNumUninitialized(expectedBytes);
		for (int32 dy = 0; dy < expectedSize; dy++) {
			int32 sy = FMath::Clamp((dy * srcH) / expectedSize, 0, srcH - 1);
			for (int32 dx = 0; dx < expectedSize; dx++) {
				int32 sx = FMath::Clamp((dx * srcW) / expectedSize, 0, srcW - 1);
				const uint8* srcPixel = rawBytes.GetData() + ((int64)sy * srcW + sx) * 4;
				uint8* dstPixel = outBytes.GetData() + ((int64)dy * expectedSize + dx) * 4;
				dstPixel[0] = srcPixel[0]; dstPixel[1] = srcPixel[1]; dstPixel[2] = srcPixel[2]; dstPixel[3] = srcPixel[3];
			}
		}
		return true;
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
	nearControlLevel = FMath::Clamp(nearControlLevel, 0, num - 1);
	nearTexelsPerElement = FMath::Max(nearTexelsPerElement, 1);
	nearBlendRadius = FMath::Clamp(nearBlendRadius, 1.f, 3.f);

	const auto& terrainTextures = map->GetTerrainTextures();
	if (!terrainTextures.empty()) {
		int32 numSlices = static_cast<int32>(terrainTextures.size());
		int32 sliceBytes = DIFFUSE_TEX_SIZE * DIFFUSE_TEX_SIZE * 4;

		TArray<UTexture2D*> diffuseTextures, normalTextures, roughnessTextures;
		diffuseTextures.SetNum(numSlices);
		normalTextures.SetNum(numSlices);
		roughnessTextures.SetNum(numSlices);
		for (auto& [textureName, textureInfo] : terrainTextures) {
			if (!diffuseTextures.IsValidIndex(textureInfo.slice)) continue;
			if (!textureInfo.diffuse.empty())
				diffuseTextures[textureInfo.slice] = LoadObject<UTexture2D>(nullptr, *FString(textureInfo.diffuse.c_str()));
			if (!textureInfo.normal.empty())
				normalTextures[textureInfo.slice] = LoadObject<UTexture2D>(nullptr, *FString(textureInfo.normal.c_str()));
			if (!textureInfo.roughness.empty())
				roughnessTextures[textureInfo.slice] = LoadObject<UTexture2D>(nullptr, *FString(textureInfo.roughness.c_str()));
		}

		// 构建TerrainDiffuseArray:逐槽位拷贝源贴图的BGRA8字节(按源尺寸最近邻重采样到DIFFUSE_TEX_SIZE),
		// 读不到数据时整层填中性灰(128,128,128,255)并打一条Warning。
		terrainDiffuseArray = NewObject<UTexture2DArray>(this, NAME_None, RF_Transient);
		terrainDiffuseArray->Filter = TF_Bilinear;
		terrainDiffuseArray->SRGB = true;
#if WITH_EDITOR
		terrainDiffuseArray->MipGenSettings = TMGS_NoMipmaps;
#endif
		terrainDiffuseArray->NeverStream = true;
		{
			FTexturePlatformData* platformData = new FTexturePlatformData();
			platformData->SizeX = DIFFUSE_TEX_SIZE;
			platformData->SizeY = DIFFUSE_TEX_SIZE;
			platformData->SetNumSlices(numSlices);
			platformData->PixelFormat = PF_B8G8R8A8;

			FTexture2DMipMap* mip = new FTexture2DMipMap();
			mip->SizeX = DIFFUSE_TEX_SIZE;
			mip->SizeY = DIFFUSE_TEX_SIZE;
			mip->SizeZ = numSlices;

			mip->BulkData.Lock(LOCK_READ_WRITE);
			uint8* destPixels = (uint8*)mip->BulkData.Realloc((int64)sliceBytes * numSlices);
			FMemory::Memzero(destPixels, (int64)sliceBytes * numSlices);

			for (int32 sliceIndex = 0; sliceIndex < numSlices; sliceIndex++) {
				uint8* slicePixels = destPixels + (int64)sliceIndex * sliceBytes;
				TArray64<uint8> srcBytes;
				if (LoadRawBGRA8(diffuseTextures[sliceIndex], DIFFUSE_TEX_SIZE, srcBytes)) {
					FMemory::Memcpy(slicePixels, srcBytes.GetData(), sliceBytes);
				}
				else {
					if (diffuseTextures[sliceIndex]) {
						UE_LOG(LogTemp, Warning, TEXT("UForeverTerrainFrameworkComponent: diffuse texture at slot %d failed to load or is not %dx%d, filling fallback gray."),
							sliceIndex, DIFFUSE_TEX_SIZE, DIFFUSE_TEX_SIZE);
					}
					for (int64 p = 0; p < (int64)DIFFUSE_TEX_SIZE * DIFFUSE_TEX_SIZE; p++) {
						slicePixels[p * 4 + 0] = 128; slicePixels[p * 4 + 1] = 128;
						slicePixels[p * 4 + 2] = 128; slicePixels[p * 4 + 3] = 255;
					}
				}
			}

			mip->BulkData.Unlock();
			platformData->Mips.Add(mip);
			terrainDiffuseArray->SetPlatformData(platformData);
			terrainDiffuseArray->UpdateResource();
		}

		// 构建TerrainNormalRoughArray:每个槽位把法线源的R/G(法线xy)和粗糙度源的R(灰度)
		// 打包进同一层,sRGB关(存的是打包数据不是颜色)。源缺失/读不到时填平的
		// 法线(128,128)+中性粗糙度(204≈0.8)。
		terrainNormalRoughArray = NewObject<UTexture2DArray>(this, NAME_None, RF_Transient);
		terrainNormalRoughArray->Filter = TF_Bilinear;
		terrainNormalRoughArray->SRGB = false;
#if WITH_EDITOR
		terrainNormalRoughArray->MipGenSettings = TMGS_NoMipmaps;
#endif
		terrainNormalRoughArray->NeverStream = true;
		{
			FTexturePlatformData* platformData = new FTexturePlatformData();
			platformData->SizeX = DIFFUSE_TEX_SIZE;
			platformData->SizeY = DIFFUSE_TEX_SIZE;
			platformData->SetNumSlices(numSlices);
			platformData->PixelFormat = PF_B8G8R8A8;

			FTexture2DMipMap* mip = new FTexture2DMipMap();
			mip->SizeX = DIFFUSE_TEX_SIZE;
			mip->SizeY = DIFFUSE_TEX_SIZE;
			mip->SizeZ = numSlices;

			mip->BulkData.Lock(LOCK_READ_WRITE);
			uint8* destPixels = (uint8*)mip->BulkData.Realloc((int64)sliceBytes * numSlices);
			FMemory::Memzero(destPixels, (int64)sliceBytes * numSlices);

			for (int32 sliceIndex = 0; sliceIndex < numSlices; sliceIndex++) {
				uint8* slicePixels = destPixels + (int64)sliceIndex * sliceBytes;
				TArray64<uint8> normalBytes, roughBytes;
				bool normalOk = LoadRawBGRA8(normalTextures[sliceIndex], DIFFUSE_TEX_SIZE, normalBytes);
				bool roughOk = LoadRawBGRA8(roughnessTextures[sliceIndex], DIFFUSE_TEX_SIZE, roughBytes);
				if (normalTextures[sliceIndex] && !normalOk) {
					UE_LOG(LogTemp, Warning, TEXT("UForeverTerrainFrameworkComponent: normal texture at slot %d failed to load or is not %dx%d, filling flat normal."),
						sliceIndex, DIFFUSE_TEX_SIZE, DIFFUSE_TEX_SIZE);
				}
				if (roughnessTextures[sliceIndex] && !roughOk) {
					UE_LOG(LogTemp, Warning, TEXT("UForeverTerrainFrameworkComponent: roughness texture at slot %d failed to load or is not %dx%d, filling neutral roughness."),
						sliceIndex, DIFFUSE_TEX_SIZE, DIFFUSE_TEX_SIZE);
				}
				for (int64 p = 0; p < (int64)DIFFUSE_TEX_SIZE * DIFFUSE_TEX_SIZE; p++) {
					// 源贴图也是BGRA8,R通道在字节下标2,G通道在字节下标1。
					uint8 normalSrcR = normalOk ? normalBytes[p * 4 + 2] : 128;
					uint8 normalSrcG = normalOk ? normalBytes[p * 4 + 1] : 128;
					uint8 roughSrcR = roughOk ? roughBytes[p * 4 + 2] : 204;
					slicePixels[p * 4 + 0] = roughSrcR;  // 输出B=粗糙度
					slicePixels[p * 4 + 1] = normalSrcG; // 输出G=法线y
					slicePixels[p * 4 + 2] = normalSrcR; // 输出R=法线x
					slicePixels[p * 4 + 3] = 255;
				}
			}

			mip->BulkData.Unlock();
			platformData->Mips.Add(mip);
			terrainNormalRoughArray->SetPlatformData(platformData);
			terrainNormalRoughArray->UpdateResource();
		}

		// 构建地形索引图(远处用):每像素存储该元素的地形类型槽位索引(PF_G8);同时顺手填好
		// nearSliceIds/nearIsConstruction(近处混合用),两份数据来自同一次扫描,不重复遍历。
		auto [mapWidth, mapHeight] = map->GetSize();
		terrainIndexMap = UTexture2D::CreateTransient(mapWidth, mapHeight, PF_G8, TEXT("TerrainIndexMap"));
		terrainIndexMap->NeverStream = true;
		terrainIndexMap->Filter = TF_Nearest;

		nearSliceIds.assign((size_t)mapWidth * mapHeight, 0);
		nearIsConstruction.assign((size_t)mapWidth * mapHeight, 0);

		FTexture2DMipMap& indexMip = terrainIndexMap->GetPlatformData()->Mips[0];
		uint8* indexPixels = (uint8*)indexMip.BulkData.Lock(LOCK_READ_WRITE);
		for (int y = 0; y < mapHeight; y++)
			for (int x = 0; x < mapWidth; x++) {
				const string& terrainName = map->GetTerrain(x, y);
				uint8 terrainIndex = 0;
				auto terrainIter = terrainTextures.find(terrainName);
				if (terrainIter != terrainTextures.end())
					terrainIndex = (uint8)FMath::Min(terrainIter->second.slice, 255);
				indexPixels[y * mapWidth + x] = terrainIndex;
				nearSliceIds[(size_t)y * mapWidth + x] = terrainIndex;
				nearIsConstruction[(size_t)y * mapWidth + x] = (terrainName == "construction") ? 1 : 0;
			}
		indexMip.BulkData.Unlock();
		terrainIndexMap->UpdateResource();

		if (terrainBaseMaterial) {
			terrainMaterial = UMaterialInstanceDynamic::Create(terrainBaseMaterial, this);
			terrainMaterial->SetTextureParameterValue(TEXT("TerrainDiffuseArray"), terrainDiffuseArray);
			terrainMaterial->SetTextureParameterValue(TEXT("TerrainNormalRoughArray"), terrainNormalRoughArray);
			terrainMaterial->SetTextureParameterValue(TEXT("TerrainIndexMap"), terrainIndexMap);
			terrainMaterial->SetVectorParameterValue(TEXT("TerrainMapSize"), FLinearColor((float)mapWidth, (float)mapHeight, 0.f, 0.f));
		}

		// 近处材质+它的两张控制纹理(持久化,原地更新,不是每次CreateTransient)。
		nearBlend = std::make_unique<NearTerrainBlend>(NearBlendParams{ nearTexelsPerElement, nearBlendRadius });
		if (nearBaseMaterial) {
			nearMaterial = UMaterialInstanceDynamic::Create(nearBaseMaterial, this);
			nearMaterial->SetTextureParameterValue(TEXT("TerrainDiffuseArray"), terrainDiffuseArray);
			nearMaterial->SetTextureParameterValue(TEXT("TerrainNormalRoughArray"), terrainNormalRoughArray);
		}

		int32 nearRegionSize = FMath::RoundToInt(currentPivots[nearControlLevel].second);
		int32 nearRes = nearRegionSize * nearTexelsPerElement;
		nearIndexMap = UTexture2D::CreateTransient(nearRes, nearRes, PF_B8G8R8A8, TEXT("NearIndexMap"));
		nearWeightMap = UTexture2D::CreateTransient(nearRes, nearRes, PF_B8G8R8A8, TEXT("NearWeightMap"));
		for (UTexture2D* tex : { nearIndexMap, nearWeightMap }) {
			if (!tex) continue;
			tex->Filter = TF_Nearest;
			tex->AddressX = TA_Clamp;
			tex->AddressY = TA_Clamp;
			tex->SRGB = false;
			tex->NeverStream = true;
		}
		if (nearMaterial) {
			nearMaterial->SetTextureParameterValue(TEXT("NearIndexMap"), nearIndexMap);
			nearMaterial->SetTextureParameterValue(TEXT("NearWeightMap"), nearWeightMap);
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
		boundary.bottom.SetNum(TERRAIN_LOD_GRID_SUBDIV + 1);
		boundary.top.SetNum(TERRAIN_LOD_GRID_SUBDIV + 1);
		boundary.left.SetNum(TERRAIN_LOD_GRID_SUBDIV + 1);
		boundary.right.SetNum(TERRAIN_LOD_GRID_SUBDIV + 1);
	}

	BuildOceanMesh();
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

	// LookupTerrain在格子局部[0,1]坐标里裁剪：碎片面积小于这个值(约1cm2)当作退化丢掉。
	constexpr float kMinLocalPieceArea = 1e-6f;
	// clipGridCell在世界坐标(UE单位)里裁剪：坐标在几十万量级，float精度只有0.03~0.06，共边/过顶点的hatch
	// 切出的碎片宽度就是这个量级的噪声，面积在10 cm2以内；而一个小格是几千cm2。
	constexpr float kMinWorldPieceArea = 10.f;
	// 单个扇形三角形的最小面积(同上世界坐标)：重复点/共线点产生的零面积三角形直接丢掉。
	constexpr float kMinWorldTriArea = 1.f;

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

	// 网格小格的三角形被hatch裁剪时用的多边形顶点：pos是世界坐标(已经含LOD接缝修正和HEIGHT_EPSILON，
	// 即这个三角形真正渲染的位置)，index是它在BuildLevel里vertices数组中的下标——三角形原有的
	// 角点直接复用网格顶点(和邻格共用，没有裂缝、法线连续)，裁剪新产生的交点index=-1，输出时才追加。
	struct FClipPoint {
		FVector pos;
		int32 index;
	};

	// SplitConvexPolygonByLine的FClipPoint版本：交点在三角形所在平面上做3D线性插值(原三角形是
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

	// 把一个凸多边形(CCW，同本文件主网格quad(v00,v11,v10)/(v00,v01,v11)那一套已验证过的绕序
	// 约定：对(左下,右下,右上,左上)这种CCW四边形要输出(v0,v2,v1)+(v0,v3,v2)而不是朴素的
	// (v0,v1,v2)+(v0,v2,v3)才能让法线朝上)扇形三角化成FTerrainTri2D列表——把朴素fan的每个
	// 三角形后两个顶点对调，泛化到任意点数的凸多边形。
	void FanTriangulate(const TArray<FVector2D>& polygon, TArray<FTerrainTri2D>& outTris) {
		for (int32 i = 1; i + 1 < polygon.Num(); i++) {
			// 多边形里有重复点/共线点时(裁剪线正好过顶点)，扇形里会出零面积三角形，跳过。
			double cross = ((double)polygon[i].X - polygon[0].X) * ((double)polygon[i + 1].Y - polygon[0].Y)
				- ((double)polygon[i + 1].X - polygon[0].X) * ((double)polygon[i].Y - polygon[0].Y);
			if (FMath::Abs(cross) * 0.5 <= kMinLocalPieceArea) continue;
			outTris.Add({ polygon[0], polygon[i + 1], polygon[i] });
		}
	}
}

void UForeverTerrainFrameworkComponent::LookupTerrain(int elemX, int elemY, FString& type,
	TArray<FTerrainTri2D>& tris) const {
	if (!map) return;

	type = FString(map->GetTerrain(elemX, elemY).c_str());
	// 挖洞条件是"construction"或这个格子有hatch：隧道口所在格子是mountain/plain，必须因hatch继续
	// 走裁剪，否则隧道段被山体实心地形挡住，详见roadnet_basic.md"隧道"一节。
	if (type != "construction" && map->GetHatches(elemX, elemY).empty()) return;

	tris.Empty();

	// 这个格子里还需要跟下一个hatch比对的剩余区域(可能不止一块)，局部坐标[0,1]x[0,1]，
	// 初始是整个格子(CCW:左下->右下->右上->左上，和主网格quad同一套绕序约定)。
	TArray<TArray<FVector2D>> workPolys;
	workPolys.Add({ {0.f,0.f}, {1.f,0.f}, {1.f,1.f}, {0.f,1.f} });

	for (const TerrainHatch& terrainHatch : map->GetHatches(elemX, elemY)) {
		const Quad& quad = terrainHatch.quad;
		float rotation = terrainHatch.rotation;
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

	// 依次按hatch矩形的4条边裁剪(任意旋转矩形的4个半平面)：某块剩余区域只要落在一条边的外侧，就确定在hatch外面，
	// 直接进survivingPieces留给下一个hatch；内侧部分继续拿下一条边判断。4条边都在内侧的才是被挖穿的洞，丢弃。
	// hatch跨几个格子都无所谓，每个格子独立按自己的[0,1]范围精确裁剪。
		TArray<TArray<FVector2D>> survivingPieces;
		for (const TArray<FVector2D>& piece : workPolys) {
			TArray<FVector2D> currentInside = piece;
			for (int32 e = 0; e < 4 && currentInside.Num() >= 3; e++) {
				TArray<FVector2D> insidePart, outsidePart;
				SplitConvexPolygonByLine(currentInside, hatchPoly[e], hatchPoly[(e + 1) % 4], hatchCenter, insidePart, outsidePart);
				if (outsidePart.Num() >= 3 && PolygonAreaAbs(outsidePart, [](const FVector2D& p) { return p; }) > kMinLocalPieceArea) {
					survivingPieces.Add(outsidePart);
				}
				currentInside = MoveTemp(insidePart);
			}
		}
		workPolys = MoveTemp(survivingPieces);
	}

	// workPolys现在就是这个格子里真正的实心地形区域(可能是好几块互不相连的多边形)，扇形三角化
	// 输出；没有hatch命中时就是整格方块，输出2个三角形。
	for (const TArray<FVector2D>& piece : workPolys) {
		FanTriangulate(piece, tris);
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
	// 所以CreateMeshSection不传UV,见.md"近处地形混合"一节。
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

	// construction格子:只在levelIdx<=1(近处精细LOD)用LookupTerrain的精确三角形整格替换(construction在平地上)；
	// 其它有hatch的格子(可能在起伏地形上)在levelIdx<=TERRAIN_HATCH_MAX_LEVEL时走clipGridCell，详见.md"挖洞"一节。
	TMap<TPair<int32, int32>, TArray<FTerrainTri2D>> constructionCache;
	auto lookupCached = [this, &constructionCache](int ex, int ey) -> const TArray<FTerrainTri2D>& {
		TPair<int32, int32> key(ex, ey);
		if (auto* found = constructionCache.Find(key)) return *found;
		FString type;
		TArray<FTerrainTri2D> tris;
		LookupTerrain(ex, ey, type, tris);
		return constructionCache.Add(key, MoveTemp(tris));
		};

	// 有hatch(且不是construction)的element里，一个网格小格(cx,cy)的两个三角形按这个element的全部
	// hatch旋转矩形裁剪，洞里的部分丢掉，洞外的部分用原三角形的平面插值出交点(高度和原渲染表面一致)，
	// 角点复用网格顶点。小格不会跨element(L0/L1的小格边长是1/16、1/8 element，网格和element对齐)。
	// 三角形和BuildLevel主循环同一对角线、同一绕序：(v00,v10,v11)/(v00,v11,v01)是CCW，扇形输出
	// (p0,p[i+1],p[i])就是主循环里的(v00,v11,v10)/(v00,v01,v11)。
	auto clipGridCell = [this, &vertices, &triangles, worldScale](int cx, int cy, int ex, int ey) {
		const auto& hatches = map->GetHatches(ex, ey);
		int32 v00 = cy * (TERRAIN_LOD_GRID_SUBDIV + 1) + cx;
		int32 v10 = v00 + 1;
		int32 v01 = v00 + (TERRAIN_LOD_GRID_SUBDIV + 1);
		int32 v11 = v01 + 1;
		const int32 cellTris[2][3] = { { v00, v10, v11 }, { v00, v11, v01 } };

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

				// 和LookupTerrain同一个思路：依次按矩形4条边分割，外侧部分确定在矩形外，收下；
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

			bool drawQuad = true;
			if (levelIdx <= TERRAIN_HATCH_MAX_LEVEL) {
				float quadCenterMapX = startX + (cx + 0.5f) * cellSize;
				float quadCenterMapY = startY + (cy + 0.5f) * cellSize;
				int ex = FMath::Clamp(FMath::FloorToInt(quadCenterMapX), 0, mapSize.first - 1);
				int ey = FMath::Clamp(FMath::FloorToInt(quadCenterMapY), 0, mapSize.second - 1);
				// "construction"格子整格交给下面LookupTerrain的精确geometry处理(平地，不需要细化高度)；
					// 其它被Map::AddHatch记过洞的格子(Roadnet隧道口等，可能在有起伏的地形上)不能整格
					// 换成"只有四角"的画法——那样会丢掉格子里的细化高度、和邻格的边对不上产生裂缝——
					// 而是只对这个小格自己的两个三角形做hatch裁剪，没被切到的小格仍用共享的网格顶点。
					if (levelIdx <= 1 && map->GetTerrain(ex, ey) == "construction") {
						lookupCached(ex, ey);
						drawQuad = false;
					}
					else if (!map->GetHatches(ex, ey).empty()) {
						clipGridCell(cx, cy, ex, ey);
						drawQuad = false;
					}
			}
			if (!drawQuad) continue;

			int32 v00 = cy * (TERRAIN_LOD_GRID_SUBDIV + 1) + cx;
			int32 v10 = cy * (TERRAIN_LOD_GRID_SUBDIV + 1) + cx + 1;
			int32 v01 = (cy + 1) * (TERRAIN_LOD_GRID_SUBDIV + 1) + cx;
			int32 v11 = (cy + 1) * (TERRAIN_LOD_GRID_SUBDIV + 1) + cx + 1;
			triangles.Add(v00); triangles.Add(v11); triangles.Add(v10);
			triangles.Add(v00); triangles.Add(v01); triangles.Add(v11);
		}
	}

	// 为每个涉及到的construction Element画出LookupTerrain裁剪出的实心地形三角形；绕序(A,B,C)
	// 已在FanTriangulate里按主网格quad同一套约定处理过，这里直接按原顺序建。
	if (levelIdx <= 1) {
		for (const auto& entry : constructionCache) {
			int32 ex = entry.Key.Key, ey = entry.Key.Value;

			for (const FTerrainTri2D& tri : entry.Value) {
				FVector2D corners[3] = { tri.A, tri.B, tri.C };
				FVector worldCorners[3];
				for (int32 k = 0; k < 3; k++) {
					float mapX = ex + corners[k].X;
					float mapY = ey + corners[k].Y;
					float h = SampleHeight(mapX, mapY) + HEIGHT_EPSILON;
					worldCorners[k] = FVector(mapX * worldScale, mapY * worldScale, h * worldScale);
				}
				// 格子局部坐标里很细的碎片，加上ex(几百)再乘1000变成世界坐标后float精度只剩0.03~0.06，
				// 会塌成两个顶点重合的零面积三角形(局部面积阈值拦不住)，按世界坐标再判一次。
				double cross = ((double)worldCorners[1].X - worldCorners[0].X) * ((double)worldCorners[2].Y - worldCorners[0].Y)
					- ((double)worldCorners[2].X - worldCorners[0].X) * ((double)worldCorners[1].Y - worldCorners[0].Y);
				if (FMath::Abs(cross) * 0.5 <= kMinWorldTriArea) continue;
				int32 baseIdx = vertices.Num();
				for (const FVector& corner : worldCorners) vertices.Add(corner);
				triangles.Add(baseIdx); triangles.Add(baseIdx + 1); triangles.Add(baseIdx + 2);
			}
		}
	}

	// ProceduralMeshComponent不会自动算法线：按面法线面积加权累加到顶点再归一化得到平滑法线;
	// 切线把世界X轴投影到切平面做Gram-Schmidt正交化，和材质里"U方向=世界X"
	// (TileUV=P*TilesPerElement,P=世界XY/1000)的约定对上，否则法线贴图凹凸方向会和纹理对不齐。
	TArray<FVector> normals;
	normals.SetNumZeroed(vertices.Num());
	for (int32 t = 0; t < triangles.Num(); t += 3) {
		const FVector& a = vertices[triangles[t]];
		const FVector& b = vertices[triangles[t + 1]];
		const FVector& c = vertices[triangles[t + 2]];
		FVector faceNormal = FVector::CrossProduct(c - a, b - a);
		normals[triangles[t]] += faceNormal;
		normals[triangles[t + 1]] += faceNormal;
		normals[triangles[t + 2]] += faceNormal;
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

	mesh->CreateMeshSection(0, vertices, triangles,
		normals, TArray<FVector2D>(), TArray<FColor>(), tangents, true);

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

	// levelIdx<=nearControlLevel的LOD都在近处控制纹理覆盖的区域内,用同一个nearMaterial
	// (纹理内容由RebuildNearControl按nearControlLevel那一级的pivot更新,不是每级各自建一份)。
	if (levelIdx <= nearControlLevel && nearMaterial) {
		mesh->SetMaterial(0, nearMaterial);
	}
	else if (terrainMaterial) {
		mesh->SetMaterial(0, terrainMaterial);
	}
}

void UForeverTerrainFrameworkComponent::RebuildNearControl(float originX, float originY, int32 sizeElements) {
	if (!map || !nearBlend || !nearIndexMap || !nearWeightMap) return;

	int32 clampedOriginX = FMath::FloorToInt(originX);
	int32 clampedOriginY = FMath::FloorToInt(originY);
	if (nearRegionPivot.first == clampedOriginX && nearRegionPivot.second == clampedOriginY) return;

	double startTime = FPlatformTime::Seconds();
	auto [mapWidth, mapHeight] = map->GetSize();

	NearBlendRegion region;
	region.originX = clampedOriginX;
	region.originY = clampedOriginY;
	region.size = sizeElements;
	region.mapWidth = mapWidth;
	region.mapHeight = mapHeight;
	region.sliceIds = nearSliceIds.data();
	region.isConstruction = nearIsConstruction.data();

	std::vector<uint8_t> outIds, outWeights;
	nearBlend->BuildRegion(region, outIds, outWeights);

	// res和GenerateTerrain里创建纹理时恒定一致，原地Realloc到同样字节数更新，不重新CreateTransient。
	int32 res = sizeElements * nearTexelsPerElement;
	int64 expectedBytes = (int64)res * res * 4;
	auto writeTexture = [expectedBytes](UTexture2D* tex, const std::vector<uint8_t>& bytes) {
		FTexturePlatformData* pd = tex->GetPlatformData();
		if (!pd || pd->Mips.Num() == 0) return;
		FTexture2DMipMap& mip = pd->Mips[0];
		mip.BulkData.Lock(LOCK_READ_WRITE);
		void* dest = mip.BulkData.Realloc(expectedBytes);
		FMemory::Memcpy(dest, bytes.data(), expectedBytes);
		mip.BulkData.Unlock();
		tex->UpdateResource();
		};
	writeTexture(nearIndexMap, outIds);
	writeTexture(nearWeightMap, outWeights);

	if (nearMaterial) {
		nearMaterial->SetVectorParameterValue(TEXT("NearRegion"),
			FLinearColor((float)clampedOriginX, (float)clampedOriginY, (float)sizeElements, 0.f));
	}

	nearRegionPivot = { clampedOriginX, clampedOriginY };
	double elapsedMs = (FPlatformTime::Seconds() - startTime) * 1000.0;
	UE_LOG(LogTemp, Log, TEXT("UForeverTerrainFrameworkComponent::RebuildNearControl origin=(%d,%d) size=%d res=%d elapsed=%.2fms"),
		clampedOriginX, clampedOriginY, sizeElements, res, elapsedMs);
}

void UForeverTerrainFrameworkComponent::BuildOceanMesh() {
	if (!map) return;

	AActor* owner = GetOwner();
	if (!owner) return;

	auto [width, height] = map->GetSize();
	const float worldScale = 1000.f;

	// 共享网格顶点:(width+1)*(height+1)个格点,高度初始0，有水的格子把4个角写成水位(相邻格水位不同时后写的覆盖)
	TArray<FVector> vertices;
	TArray<FVector2D> uvs;
	vertices.Reserve((width + 1) * (height + 1));
	uvs.Reserve((width + 1) * (height + 1));
	for (int vy = 0; vy <= height; vy++)
		for (int vx = 0; vx <= width; vx++) {
			vertices.Add(FVector(vx * worldScale, vy * worldScale, 0.f));
			uvs.Add(FVector2D(vx, vy));
		}

	// 只为有水面的格子(map->GetWater(x,y).first，海洋和河流)生成两个三角形
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

	// 河流水面每个格子高度不同会倾斜，必须带法线：按面法线面积加权累加到顶点再归一化(同BuildLevel)。
	TArray<FVector> normals;
	normals.SetNumZeroed(vertices.Num());
	for (int32 t = 0; t < triangles.Num(); t += 3) {
		const FVector& a = vertices[triangles[t]];
		const FVector& b = vertices[triangles[t + 1]];
		const FVector& c = vertices[triangles[t + 2]];
		FVector faceNormal = FVector::CrossProduct(c - a, b - a);
		normals[triangles[t]] += faceNormal;
		normals[triangles[t + 1]] += faceNormal;
		normals[triangles[t + 2]] += faceNormal;
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

	if (!oceanMesh) {
		oceanMesh = NewObject<UProceduralMeshComponent>(owner, TEXT("OceanMesh"));
		oceanMesh->SetupAttachment(owner->GetRootComponent());
		oceanMesh->RegisterComponent();
		owner->AddInstanceComponent(oceanMesh);
	}

	oceanMesh->CreateMeshSection(0, vertices, triangles,
		normals, uvs, TArray<FColor>(), tangents, false);

	if (!oceanMaterial && oceanBaseMaterial) {
		oceanMaterial = UMaterialInstanceDynamic::Create(oceanBaseMaterial, this);
	}
	if (oceanMaterial)
		oceanMesh->SetMaterial(0, oceanMaterial);
}

// 隧道口侧壁：对每个wall=true的hatch(RoadnetMod::AddTunnelHatch)，在洞的两条长边(沿rotation，横向
// 偏移+-sizeY/2)各做一条竖直的墙；不加两个端头(起点端是和地面齐平的路面入口，终点端是隧道本身，隧道里
// 的墙和顶棚用资产)。墙顶=地形高度，墙底=洞底(floorStart->floorEnd，smoothstep)再往下埋一点；做成有
// 厚度的小板(朝洞内的内面+朝洞外的外面+顶面)，内面可见、可碰撞。其它hatch(wall=false)一概没有墙。
#define TUNNEL_WALL_SEGMENT 0.25f       // 沿长度的断面间距(地图element单位)，墙顶跟着地形起伏
#define TUNNEL_WALL_THICKNESS 0.1f      // 墙厚度
#define TUNNEL_WALL_TOP_INSET 0.005f    // 墙顶比地形低多少(10m*0.005=5cm)，保证墙顶不会冒出地面
#define TUNNEL_WALL_BOTTOM_EXTRA 0.1f   // 墙底比洞底再往下埋多少(埋在路面下)

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
				// 墙顶比地形低一点，内外两侧各自取自己位置的地形高度再取较低的，外侧的墙板往洞外延伸的那一
				// 部分在地形下面，不能因为地形往外变低就冒出地面。
				float inner0 = SampleHeight(p0.X, p0.Y) - TUNNEL_WALL_TOP_INSET;
				float inner1 = SampleHeight(p1.X, p1.Y) - TUNNEL_WALL_TOP_INSET;
				float top0 = inner0, top1 = inner1;
				float outerTop0 = FMath::Min(inner0, SampleHeight(o0.X, o0.Y) - TUNNEL_WALL_TOP_INSET);
				float outerTop1 = FMath::Min(inner1, SampleHeight(o1.X, o1.Y) - TUNNEL_WALL_TOP_INSET);
				// 这一段地形不比洞底高(洞底和地面齐平的入口附近)就不需要墙。
				if (top0 <= floor0 && top1 <= floor1) continue;
				float bottom0 = floor0 - TUNNEL_WALL_BOTTOM_EXTRA, bottom1 = floor1 - TUNNEL_WALL_BOTTOM_EXTRA;
				auto world = [&](const FVector2D& p, float z) { return FVector(p.X * worldScale, p.Y * worldScale, z * worldScale); };

				addQuad(world(p0, bottom0), world(p1, bottom1), world(p1, top1), world(p0, top0), inward);
				addQuad(world(o0, bottom0), world(o1, bottom1), world(o1, outerTop1), world(o0, outerTop0), outward);
				addQuad(world(p0, top0), world(p1, top1), world(o1, outerTop1), world(o0, outerTop0), FVector::UpVector);
			}
		}
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
