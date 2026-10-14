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
// 每级LOD mesh的网格细分数(每级都是这个数,级与级之间只有size不一样)——原来是32
// (33x33个顶点)。试过提到4倍(128)太卡，先退到2倍(64,65x65个顶点)看效果。所有跟这个数
// 绑定的魔法数字(33、32、8)都已经改成由这个常量推导，调这一个数就行，不用逐个改。
#define TERRAIN_LOD_GRID_SUBDIV 64

using namespace std;

UForeverTerrainFrameworkComponent::UForeverTerrainFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> terrainMatFinder(
		TEXT("/Game/Asset/Materials/TemplateFar.TemplateFar"));
	if (terrainMatFinder.Succeeded()) {
		terrainBaseMaterial = terrainMatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> nearMatFinder(
		TEXT("/Game/Asset/Materials/TemplateNear.TemplateNear"));
	if (nearMatFinder.Succeeded()) {
		nearBaseMaterial = nearMatFinder.Object;
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
	// 读取srcTexture的原始像素数据，只有分辨率恰好是expectedSize×expectedSize时才成功——
	// 这是贴图数组构建代码本来就有的校验逻辑(以前只检查字节数够不够，现在统一成"分辨率必须
	// 正好符合"，TerrainDiffuseArray/TerrainNormalRoughArray两个数组共用这一份)。
	// 按源贴图实际分辨率重采样到expectedSize x expectedSize(最近邻,BGRA8逐像素拷贝)——不
	// 要求美术资产必须精确准备成expectedSize,源贴图比目标小/大/不是正方形都能处理,只有真正
	// 读不到源数据(纹理空/没有mip)才失败。对所有地形diffuse/normal/roughness贴图通用,不是
	// 只为某一种地形特判。
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

		// 构建TerrainDiffuseArray:逐槡位直接拷贝源贴图的BGRA8字节,分辨率不对/加载失败时
		// 整层填中性灰(128,128,128,255),同时打一条Warning——以前这里检查不过就直接留黑,
		// 不会报错也看不出哪里错了。
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

		// 构建TerrainNormalRoughArray:每个槡位把法线源的R/G(法线xy)和粗糙度源的R(灰度)
		// 打包进同一层,sRGB关(存的是打包数据不是颜色)。两个源都缺/都不合规格时整层填平的
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

		// 构建地形索引图(远处用):每像素存储该元素的地形类型槡位索引(PF_G8);同时顺手填好
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

	const float cellSize = size / (float)TERRAIN_LOD_GRID_SUBDIV;
	const float startX = pos.first * (size / 2.f) - size / 2.f;
	const float startY = pos.second * (size / 2.f) - size / 2.f;
	const float worldScale = 1000.f;

	// 材质不读网格UV,全部用世界坐标(AbsoluteWorldPosition/1000=地图element坐标)采样,
	// 不需要再给顶点算UV——这样L0~L2的主网格和construction/挖洞三角形(以前UV约定不一样,
	// 混在同一个section/材质里会读出回绕后的随机内容,是一个确定性bug)天然统一,见
	// ForeverTerrainFrameworkComponent.md"近处地形混合"一节。
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

	const int quadrantSize = TERRAIN_LOD_GRID_SUBDIV / 4; // childPos是4x4(16格)的子级覆盖位掩码,跟细分数无关,一直是4x4
	for (int cy = 0; cy < TERRAIN_LOD_GRID_SUBDIV; cy++) {
		for (int cx = 0; cx < TERRAIN_LOD_GRID_SUBDIV; cx++) {
			int s = (cy / quadrantSize) * 4 + (cx / quadrantSize);
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

			int32 v00 = cy * (TERRAIN_LOD_GRID_SUBDIV + 1) + cx;
			int32 v10 = cy * (TERRAIN_LOD_GRID_SUBDIV + 1) + cx + 1;
			int32 v01 = (cy + 1) * (TERRAIN_LOD_GRID_SUBDIV + 1) + cx;
			int32 v11 = (cy + 1) * (TERRAIN_LOD_GRID_SUBDIV + 1) + cx + 1;
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
				}
				triangles.Add(baseIdx); triangles.Add(baseIdx + 1); triangles.Add(baseIdx + 2);
			}
		}
	}

	// 顶点法线/切线之前一直传空数组,ProceduralMeshComponent不会帮你算,相当于整张地形
	// (尤其是山体陡坡)一直在用未定义的法线参与光照——这里按面法线面积加权累加到顶点、
	// 再归一化得到平滑法线;切线不follow网格UV(已经不读UV了),而是把世界X轴投影到切平面上
	// 做Gram-Schmidt正交化,和材质里"U方向=世界X"(TileUV=P*TilesPerElement,P=世界XY/1000)
	// 的纹理方向约定对上,否则法线贴图的凹凸方向会跟纹理贴图对不齐。
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

	// res和GenerateTerrain里创建这两张纹理时用的res恒定一致(sizeElements/
	// nearTexelsPerElement都不会在运行时变)，Lock后直接Realloc到同样的字节数，和diffuse
	// 数组构建时的写法同一个约定——原地更新，不是每次CreateTransient新纹理。
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

	// 水面现在会倾斜(海洋本身还是平的,但河流水面每个格子高度不一样)，之前传空法线数组
	// 靠材质/引擎兜底在平面上还看不出问题，倾斜之后没有法线光照会不正常——按面法线面积
	// 加权累加到顶点、归一化，和BuildLevel地形主网格那次修法线同一套手法。
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
