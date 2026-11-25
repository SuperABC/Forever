#include "Framework/TerrainMaterialBuilder.h"

#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#include "map/map.h"
#include "map/terrain_coverage.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>

DEFINE_LOG_CATEGORY_STATIC(LogTerrainMaterial, Log, All);

namespace {
	enum class ESelector : int32 { None = 0, Slope = 1, WaterDepth = 2 };

	struct FLayerSpec {
		FString name;
		FString diffuse, normal, orm, roughness;
		float tileMeters = 4.f;
		bool triplanar = false;
	};

	// 兜底材质：没有声明材质的地形(TerrainMaterialSpec::defined为false——没有mod的默认地形、没调SetMaterial的mod地形)都用它。
	// 贴图是工程自带的/Game/Asset/Terrain/Plain(pak.bat会预处理这个目录)。
	const TerrainMaterialSpec& FallbackTerrainMaterial() {
		static const TerrainMaterialSpec material = [] {
			TerrainMaterialLayer layer;
			layer.name = "default_plain";
			layer.diffuse = "/Game/Asset/Terrain/Plain/PlainDiffuse.PlainDiffuse";
			layer.normal = "/Game/Asset/Terrain/Plain/PlainNormal.PlainNormal";
			layer.roughness = "/Game/Asset/Terrain/Plain/PlainRoughness.PlainRoughness";
			layer.tileMeters = 10.f;
			TerrainMaterialSpec spec;
			spec.defined = true;
			spec.layers = { layer };
			return spec;
		}();
		return material;
	}

	// 层贴图数组边长(必须等于prepare_terrain_textures.py里的RES)、覆盖图精度/模糊(材质的整体质量参数，改了要重启)
	constexpr int32 kArrayResolution = 1024;
	constexpr int32 kCoverageTexelsPerElement = 2;
	constexpr float kCoverageSigma = 1.f; // element

	// 一个层的三张贴图cook后的压缩块：每个mip一块字节(整张1024x1024贴图的这一级mip)，原样拷进数组的切片，不碰像素。
	// 格式由Tools/TerrainMaterial/prepare_terrain_textures.py保证：漫反射BC7(sRGB)、法线BC5、遮罩BC7(线性，ORM的R=AO,G=粗糙度)。
	struct FLayerBlocks {
		bool valid = false;
		bool hasAo = true; // 遮罩贴图的R是AO(ORM)；单独的粗糙度图没有AO
		std::vector<std::vector<uint8_t>> diffuse, normal, mask;
	};

	int64 MipBytes(EPixelFormat format, int32 width, int32 height) {
		const FPixelFormatInfo& info = GPixelFormats[format];
		return (int64)FMath::DivideAndRoundUp(width, (int32)info.BlockSizeX) * FMath::DivideAndRoundUp(height, (int32)info.BlockSizeY) * info.BlockBytes;
	}

	// 取贴图平台数据里每个mip的原始(压缩)字节。编辑器走DDC，打包版走资产里内联的bulk data(NeverStream)，同一个接口。
	bool ReadCompressedMips(UTexture2D* texture, EPixelFormat expectedFormat, int32 res, std::vector<std::vector<uint8_t>>& out, FString& why) {
	#if WITH_EDITOR
		texture->FinishCachePlatformData();
	#endif
		FTexturePlatformData* platformData = texture->GetPlatformData();
		if (!platformData) { why = TEXT("has no platform data"); return false; }
		const int32 mipCount = FullMipCount(res, res);
		if (platformData->PixelFormat != expectedFormat) {
			why = FString::Printf(TEXT("is %s, expected %s"), GPixelFormats[platformData->PixelFormat].Name, GPixelFormats[expectedFormat].Name);
			return false;
		}
		if (platformData->Mips.Num() != mipCount || platformData->Mips[0].SizeX != res || platformData->Mips[0].SizeY != res || platformData->GetNumSlices() != 1) {
			why = FString::Printf(TEXT("has %d mips starting at %dx%d, expected %d mips starting at %dx%d"), platformData->Mips.Num(),
				platformData->Mips.Num() ? platformData->Mips[0].SizeX : 0, platformData->Mips.Num() ? platformData->Mips[0].SizeY : 0, mipCount, res, res);
			return false;
		}
		// GetMipData给每个mip malloc一块缓冲(调用方释放)；大小由格式+尺寸决定(格式/尺寸前面已经核对过)，加载失败的mip指针为空。
		TArray<void*> pointers;
		pointers.SetNumZeroed(mipCount + 1);
		texture->GetMipData(0, pointers.GetData());
		bool ok = true;
		out.assign(mipCount, std::vector<uint8_t>());
		for (int32 m = 0; ok && m < mipCount; m++) {
			const int64 expected = MipBytes(expectedFormat, FMath::Max(1, res >> m), FMath::Max(1, res >> m));
			if (!pointers[m]) {
				why = FString::Printf(TEXT("mip %d is missing"), m);
				ok = false;
				break;
			}
			out[m].assign((const uint8_t*)pointers[m], (const uint8_t*)pointers[m] + expected);
		}
		for (void* pointer : pointers) {
			if (pointer) FMemory::Free(pointer);
		}
		return ok;
	}

	UTexture* LoadAsset(const FString& path) {
		return path.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *path);
	}

	enum class ELayerRole { Diffuse, Normal, Mask };

#if WITH_EDITOR
	// 编辑器/PIE不经过pak.bat，没人替它跑prepare_terrain_textures.py：这里在内存里把层贴图设成同样的属性(规则必须和脚本一致)，
	// 不存盘(不动工作区的.uasset，也不把资产标脏)；打包仍靠pak.bat里的脚本正式存盘。属性有变就重新生成平台数据(首次几秒，之后进DDC)。
	// 源图不是方形2的幂或小于kArrayResolution改不了，返回false让调用方报错。
	bool PrepareTextureInEditor(UTexture2D* texture, ELayerRole role, FString& why) {
		const int32 sourceX = texture->Source.GetSizeX();
		const int32 sourceY = texture->Source.GetSizeY();
		if (sourceX != sourceY || !FMath::IsPowerOfTwo(sourceX) || sourceX < kArrayResolution) {
			why = FString::Printf(TEXT("source is %dx%d, must be square, power of two and >= %d"), sourceX, sourceY, kArrayResolution);
			return false;
		}
		const TextureCompressionSettings compression = role == ELayerRole::Normal ? TC_Normalmap : TC_BC7;
		const bool srgb = role == ELayerRole::Diffuse;
		if (texture->CompressionSettings == compression && texture->SRGB == srgb && texture->MaxTextureSize == kArrayResolution && texture->NeverStream) {
			return true;
		}
		UE_LOG(LogTerrainMaterial, Warning, TEXT("terrain layer texture %s is not in the runtime format (compression %d, sRGB %d, max size %d, never stream %d), fixed in memory (not saved; pak.bat's prepare_terrain_textures.py does the persistent change)"),
			*texture->GetPathName(), (int32)texture->CompressionSettings, (int32)texture->SRGB, texture->MaxTextureSize, (int32)texture->NeverStream);
		texture->PreEditChange(nullptr);
		texture->CompressionSettings = compression;
		texture->SRGB = srgb;
		texture->MaxTextureSize = kArrayResolution;
		texture->NeverStream = true;
		texture->PostEditChange();
		return true;
	}
#endif

	bool LoadLayerBlocks(const FLayerSpec& spec, int32 res, FLayerBlocks& out) {
		auto fail = [&](const FString& why) {
			UE_LOG(LogTerrainMaterial, Error, TEXT("terrain layer \"%s\": %s"), *spec.name, *why);
			return false;
		};
		if (spec.diffuse.IsEmpty() || spec.normal.IsEmpty() || (spec.orm.IsEmpty() == spec.roughness.IsEmpty())) {
			return fail(TEXT("needs diffuse, normal and exactly one of orm / roughness"));
		}
		struct FSource { const FString& path; EPixelFormat format; ELayerRole role; std::vector<std::vector<uint8_t>>* mips; };
		const FString& maskPath = spec.orm.IsEmpty() ? spec.roughness : spec.orm;
		const FSource sources[3] = { { spec.diffuse, PF_BC7, ELayerRole::Diffuse, &out.diffuse }, { spec.normal, PF_BC5, ELayerRole::Normal, &out.normal }, { maskPath, PF_BC7, ELayerRole::Mask, &out.mask } };
		for (const FSource& source : sources) {
			UTexture2D* texture = Cast<UTexture2D>(LoadAsset(source.path));
			if (!texture) return fail(FString::Printf(TEXT("%s could not be loaded"), *source.path));
			FString why;
#if WITH_EDITOR
			if (!PrepareTextureInEditor(texture, source.role, why)) {
				return fail(FString::Printf(TEXT("%s %s"), *source.path, *why));
			}
#endif
			if (!ReadCompressedMips(texture, source.format, res, *source.mips, why)) {
				return fail(FString::Printf(TEXT("%s %s (run Tools/TerrainMaterial/prepare_terrain_textures.py on its folder)"), *source.path, *why));
			}
		}
		out.hasAo = !spec.orm.IsEmpty();
		out.valid = true;
		return true;
	}

	// 通用纹理数组：format决定每个slice每级mip的字节数(压缩格式按4x4块算)；getData返回nullptr表示这个slice填0。
	UTexture2DArray* MakeTextureArray(UObject* outer, int32 width, int32 height, int32 numSlices, int32 numMips, EPixelFormat format,
		const std::function<const std::vector<uint8_t>*(int32, int32)>& getData, bool srgb, TextureAddress address, TextureCompressionSettings compression) {
		UTexture2DArray* texture = NewObject<UTexture2DArray>(outer, NAME_None, RF_Transient);
		texture->Filter = TF_Default; // 跟随贴图组设置(各向异性)
		texture->SRGB = srgb;
		texture->CompressionSettings = compression;
		texture->AddressX = address;
		texture->AddressY = address;
		texture->NeverStream = true;
#if WITH_EDITOR
		texture->MipGenSettings = TMGS_LeaveExistingMips;
#endif
		FTexturePlatformData* platformData = new FTexturePlatformData();
		platformData->SizeX = width;
		platformData->SizeY = height;
		platformData->SetNumSlices(numSlices);
		platformData->PixelFormat = format;
		for (int32 mipIndex = 0; mipIndex < numMips; mipIndex++) {
			const int32 mw = FMath::Max(1, width >> mipIndex), mh = FMath::Max(1, height >> mipIndex);
			const int64 sliceBytes = MipBytes(format, mw, mh);
			FTexture2DMipMap* mip = new FTexture2DMipMap();
			mip->SizeX = mw;
			mip->SizeY = mh;
			mip->SizeZ = numSlices;
			mip->BulkData.Lock(LOCK_READ_WRITE);
			uint8* dest = (uint8*)mip->BulkData.Realloc(sliceBytes * numSlices);
			for (int32 slice = 0; slice < numSlices; slice++) {
				const std::vector<uint8_t>* data = getData(slice, mipIndex);
				if (data) {
					check((int64)data->size() == sliceBytes);
					FMemory::Memcpy(dest + sliceBytes * slice, data->data(), sliceBytes);
				} else {
					FMemory::Memzero(dest + sliceBytes * slice, sliceBytes);
				}
			}
			mip->BulkData.Unlock();
			platformData->Mips.Add(mip);
		}
		texture->SetPlatformData(platformData);
		texture->UpdateResource();
		return texture;
	}

	UTexture2D* MakeFloatTable(int32 width, int32 height, const TArray<FVector4f>& texels, const TCHAR* name) {
		UTexture2D* texture = UTexture2D::CreateTransient(width, height, PF_A32B32G32R32F, FName(name));
		if (!texture) return nullptr;
		texture->NeverStream = true;
		texture->Filter = TF_Nearest;
		texture->AddressX = TA_Clamp;
		texture->AddressY = TA_Clamp;
		texture->SRGB = false;
#if WITH_EDITOR
		texture->MipGenSettings = TMGS_NoMipmaps;
#endif
		FTexture2DMipMap& mip = texture->GetPlatformData()->Mips[0];
		void* dest = mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(dest, texels.GetData(), (int64)texels.Num() * sizeof(FVector4f));
		mip.BulkData.Unlock();
		texture->UpdateResource();
		return texture;
	}

	// 256x256四通道可平铺噪声(BGRA8)：4个倍频(8/16/32/64格)的值噪声，每个通道独立。固定哈希，不用游戏随机数——纯视觉扰动。
	UTexture2D* MakeNoiseTexture() {
		constexpr int32 N = 256;
		auto hash01 = [](uint32 ix, uint32 iy, uint32 octave, uint32 channel) {
			uint32 h = ix * 374761393u + iy * 668265263u + octave * 2246822519u + channel * 3266489917u + 0x9E3779B9u;
			h = (h ^ (h >> 13)) * 1274126177u;
			h ^= h >> 16;
			return (h & 0xFFFFFFu) / 16777215.f;
		};
		std::vector<uint8_t> pixels((size_t)N * N * 4);
		const int32 lattice[4] = { 8, 16, 32, 64 };
		const float amplitude[4] = { 0.5f, 0.25f, 0.125f, 0.0625f };
		for (int32 y = 0; y < N; y++) {
			for (int32 x = 0; x < N; x++) {
				float channels[4];
				for (int32 c = 0; c < 4; c++) {
					float sum = 0.f;
					for (int32 o = 0; o < 4; o++) {
						const float fx = (float)x / N * lattice[o], fy = (float)y / N * lattice[o];
						const int32 ix = (int32)std::floor(fx), iy = (int32)std::floor(fy);
						float tx = fx - ix, ty = fy - iy;
						tx = tx * tx * (3.f - 2.f * tx);
						ty = ty * ty * (3.f - 2.f * ty);
						const uint32 x0 = ix % lattice[o], x1 = (ix + 1) % lattice[o], y0 = iy % lattice[o], y1 = (iy + 1) % lattice[o];
						const float a = hash01(x0, y0, o, c), b = hash01(x1, y0, o, c), cc = hash01(x0, y1, o, c), d = hash01(x1, y1, o, c);
						sum += amplitude[o] * FMath::Lerp(FMath::Lerp(a, b, tx), FMath::Lerp(cc, d, tx), ty);
					}
					// 多倍频平均后对比度偏低，围绕0.5拉开
					channels[c] = FMath::Clamp((sum / 0.9375f - 0.5f) * 1.8f + 0.5f, 0.f, 1.f);
				}
				uint8_t* px = &pixels[((size_t)y * N + x) * 4];
				px[2] = (uint8_t)(channels[0] * 255.f + 0.5f); // R
				px[1] = (uint8_t)(channels[1] * 255.f + 0.5f); // G
				px[0] = (uint8_t)(channels[2] * 255.f + 0.5f); // B
				px[3] = (uint8_t)(channels[3] * 255.f + 0.5f); // A
			}
		}
		UTexture2D* texture = UTexture2D::CreateTransient(N, N, PF_B8G8R8A8, TEXT("TerrainNoise"));
		if (!texture) return nullptr;
		texture->NeverStream = true;
		texture->Filter = TF_Bilinear;
		texture->AddressX = TA_Wrap;
		texture->AddressY = TA_Wrap;
		texture->SRGB = false;
#if WITH_EDITOR
		texture->MipGenSettings = TMGS_NoMipmaps;
#endif
		FTexture2DMipMap& mip = texture->GetPlatformData()->Mips[0];
		void* dest = mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(dest, pixels.data(), pixels.size());
		mip.BulkData.Unlock();
		texture->UpdateResource();
		return texture;
	}
}

uint8 FTerrainMaterialBuilder::EncodeWaterDepth(float depthMeters) {
	const float t = (depthMeters - WaterDepthMinMeters) / (WaterDepthMaxMeters - WaterDepthMinMeters);
	return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(t * 255.f), 0, 255));
}

void FTerrainMaterialBuilder::BuildWaterLevelField(Map* map, int32 radius, std::vector<float>& outField) {
	outField.clear();
	if (!map) return;
	const auto [width, height] = map->GetSize();
	outField.assign((size_t)width * height, NoWater);
	std::vector<int32> queue;
	std::vector<uint8_t> steps((size_t)width * height, 0);
	std::vector<char> visited((size_t)width * height, 0);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			const auto [hasWater, level] = map->GetWater(x, y);
			if (!hasWater) continue;
			const size_t index = (size_t)y * width + x;
			outField[index] = level;
			visited[index] = 1;
			queue.push_back(static_cast<int32>(index));
		}
	}
	for (size_t head = 0; head < queue.size(); head++) {
		const int32 cur = queue[head];
		const int cx = cur % width, cy = cur / width;
		if (steps[cur] >= radius) continue;
		const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
		for (int k = 0; k < 4; k++) {
			const int nx = cx + dx[k], ny = cy + dy[k];
			if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
			const size_t next = (size_t)ny * width + nx;
			if (visited[next]) continue;
			visited[next] = 1;
			steps[next] = steps[cur] + 1;
			outField[next] = outField[cur];
			queue.push_back(static_cast<int32>(next));
		}
	}
}

UMaterialInstanceDynamic* FTerrainMaterialBuilder::Build(UObject* outer, Map* map, UMaterialInterface* baseMaterial) {
	if (!map || !outer) return nullptr;
	if (!baseMaterial) {
		UE_LOG(LogTerrainMaterial, Error, TEXT("terrain base material /Game/Asset/Terrain/M_Terrain is missing (run Tools/TerrainMaterial/build_terrain_material.py)"));
		return nullptr;
	}
	const double startTime = FPlatformTime::Seconds();

	// ---- 扫描地图里实际出现的地形种类(按id排序，下标稳定)
	const auto [mapWidth, mapHeight] = map->GetSize();
	std::set<std::string> idSet;
	for (int y = 0; y < mapHeight; y++)
		for (int x = 0; x < mapWidth; x++) idSet.insert(map->GetTerrainRef(x, y));
	std::vector<std::string> typeIds(idSet.begin(), idSet.end());
	if (static_cast<int32>(typeIds.size()) > MaxTerrainTypes) {
		UE_LOG(LogTerrainMaterial, Error, TEXT("map uses %d terrain types but the material supports %d; the extra types are drawn as the last supported type"),
			(int32)typeIds.size(), MaxTerrainTypes);
	}
	const int32 typeCount = FMath::Min((int32)typeIds.size(), MaxTerrainTypes);
	std::map<std::string, uint8_t> typeIndex;
	for (int32 i = 0; i < (int32)typeIds.size(); i++) typeIndex[typeIds[i]] = static_cast<uint8_t>(FMath::Min(i, typeCount - 1));

	// ---- 层：只加载实际用到的，按首次出现顺序编号。材质声明来自各地形mod(TerrainMod::SetMaterial，Map::GetTerrainAppearances收集)，没声明的用FallbackTerrainMaterial。
	const auto& appearances = map->GetTerrainAppearances();
	TArray<FString> layerOrder;
	TMap<FString, FLayerSpec> layerSpecs;
	TMap<FString, int32> layerIndexByName;
	struct FResolvedType { int32 layerA = -1, layerB = -1; ESelector selector = ESelector::None; float r0 = 0, r1 = 1; };
	TArray<FResolvedType> resolved;
	resolved.SetNum(typeCount);
	for (int32 i = 0; i < typeCount; i++) {
		const FString id = UTF8_TO_TCHAR(typeIds[i].c_str());
		const auto appearance = appearances.find(typeIds[i]);
		// 没声明材质的地形统一用兜底材质，不算错。
		const bool declared = appearance != appearances.end() && appearance->second.material.defined;
		if (!declared) UE_LOG(LogTerrainMaterial, Log, TEXT("terrain \"%s\" declares no material, using the default one"), *id);
		const TerrainMaterialSpec& material = declared ? appearance->second.material : FallbackTerrainMaterial();
		const bool needsTwo = material.selector != TerrainMaterialSelector::None;
		if (material.layers.size() != (needsTwo ? 2u : 1u)) {
			UE_LOG(LogTerrainMaterial, Error, TEXT("terrain \"%s\" material needs %d layer(s) for its selector; it is drawn gray"), *id, needsTwo ? 2 : 1);
			continue;
		}
		int32 indices[2] = { -1, -1 };
		for (size_t k = 0; k < material.layers.size(); k++) {
			const TerrainMaterialLayer& src = material.layers[k];
			const FString layerName = UTF8_TO_TCHAR(src.name.c_str());
			int32* existing = layerIndexByName.Find(layerName);
			if (!existing) {
				FLayerSpec spec;
				spec.name = layerName;
				spec.diffuse = UTF8_TO_TCHAR(src.diffuse.c_str());
				spec.normal = UTF8_TO_TCHAR(src.normal.c_str());
				spec.orm = UTF8_TO_TCHAR(src.orm.c_str());
				spec.roughness = UTF8_TO_TCHAR(src.roughness.c_str());
				spec.tileMeters = src.tileMeters;
				spec.triplanar = src.triplanar;
				layerSpecs.Add(layerName, spec);
				existing = &layerIndexByName.Add(layerName, layerOrder.Num());
				layerOrder.Add(layerName);
			}
			indices[k] = *existing;
		}
		resolved[i] = { indices[0], indices[1], static_cast<ESelector>(static_cast<int>(material.selector)), material.range0, material.range1 };
	}

	const int32 res = kArrayResolution;
	const int32 layerCount = FMath::Max(layerOrder.Num(), 1);
	std::vector<FLayerBlocks> blocks(layerCount);
	for (int32 l = 0; l < layerOrder.Num(); l++) {
		if (!LoadLayerBlocks(layerSpecs[layerOrder[l]], res, blocks[l])) {
			// 该层不可用：引用它的地形渲染成灰
			for (FResolvedType& type : resolved) {
				if (type.layerA == l || type.layerB == l) type = FResolvedType();
			}
		}
	}
	const int32 mipCount = FullMipCount(res, res);
	// 不可用/不存在的层槽位填0(没有地形会引用它们)，保证数组形状完整
	UTexture2DArray* diffuseArray = MakeTextureArray(outer, res, res, layerCount, mipCount, PF_BC7,
		[&](int32 slice, int32 mip) -> const std::vector<uint8_t>* { return blocks[slice].valid ? &blocks[slice].diffuse[mip] : nullptr; }, true, TA_Wrap, TC_BC7);
	UTexture2DArray* normalArray = MakeTextureArray(outer, res, res, layerCount, mipCount, PF_BC5,
		[&](int32 slice, int32 mip) -> const std::vector<uint8_t>* { return blocks[slice].valid ? &blocks[slice].normal[mip] : nullptr; }, false, TA_Wrap, TC_Normalmap);
	UTexture2DArray* maskArray = MakeTextureArray(outer, res, res, layerCount, mipCount, PF_BC7,
		[&](int32 slice, int32 mip) -> const std::vector<uint8_t>* { return blocks[slice].valid ? &blocks[slice].mask[mip] : nullptr; }, false, TA_Wrap, TC_BC7);

	// ---- 覆盖图
	std::vector<uint8_t> ids((size_t)mapWidth * mapHeight);
	for (int y = 0; y < mapHeight; y++)
		for (int x = 0; x < mapWidth; x++) ids[(size_t)y * mapWidth + x] = typeIndex[map->GetTerrainRef(x, y)];
	TerrainCoverageParams coverageParams;
	coverageParams.texelsPerElement = kCoverageTexelsPerElement;
	coverageParams.sigma = kCoverageSigma;
	TerrainCoverage coverage = BuildTerrainCoverage(ids.data(), mapWidth, mapHeight, typeCount, coverageParams);
	// 覆盖图字节是RGBA顺序(通道c=地形种类slice*4+c)，而纹理是PF_B8G8R8A8：不交换R/B的话着色器读到的.x/.z会互换(0号和2号地形串台)。
	for (auto& sliceMips : coverage.data)
		for (auto& mip : sliceMips)
			for (size_t i = 0; i + 3 < mip.size(); i += 4) std::swap(mip[i], mip[i + 2]);
	UTexture2DArray* coverageArray = MakeTextureArray(outer, coverage.width, coverage.height, coverage.sliceCount, coverage.mipCount, PF_B8G8R8A8,
		[&](int32 slice, int32 mip) -> const std::vector<uint8_t>* { return &coverage.data[slice][mip]; }, false, TA_Clamp, TC_VectorDisplacementmap);

	// ---- 表：类型表(2 x 8)、层表(2 x layerCount)
	TArray<FVector4f> typeTexels;
	typeTexels.SetNumZeroed(2 * MaxTerrainTypes);
	for (int32 i = 0; i < typeCount; i++) {
		const FResolvedType& type = resolved[i];
		typeTexels[i * 2 + 0] = FVector4f((float)type.layerA, (float)type.layerB, (float)(int32)type.selector, 0.f);
		typeTexels[i * 2 + 1] = FVector4f(type.r0, type.r1, 0.f, 0.f);
	}
	for (int32 i = typeCount; i < MaxTerrainTypes; i++) typeTexels[i * 2 + 0] = FVector4f(-1.f, -1.f, 0.f, 0.f);
	TArray<FVector4f> layerTexels;
	layerTexels.SetNumZeroed(layerCount);
	for (int32 l = 0; l < layerCount; l++) {
		const bool real = l < layerOrder.Num();
		const FLayerSpec* spec = real ? layerSpecs.Find(layerOrder[l]) : nullptr;
		// 层表(1 x layerCount)：tile(cm)，三向投影标志，遮罩贴图有AO标志
		layerTexels[l] = FVector4f(spec ? spec->tileMeters * 100.f : 400.f, (spec && spec->triplanar) ? 1.f : 0.f, blocks[l].hasAo ? 1.f : 0.f, 0.f);
	}
	UTexture2D* typeTable = MakeFloatTable(2, MaxTerrainTypes, typeTexels, TEXT("TerrainTypeTable"));
	UTexture2D* layerTable = MakeFloatTable(1, layerCount, layerTexels, TEXT("TerrainLayerTable"));
	UTexture2D* noise = MakeNoiseTexture();
	if (!diffuseArray || !normalArray || !maskArray || !coverageArray || !typeTable || !layerTable || !noise) {
		UE_LOG(LogTerrainMaterial, Error, TEXT("failed to create terrain textures"));
		return nullptr;
	}

	UMaterialInstanceDynamic* material = UMaterialInstanceDynamic::Create(baseMaterial, outer);
	material->SetTextureParameterValue(TEXT("TerrainDiffuseArray"), diffuseArray);
	material->SetTextureParameterValue(TEXT("TerrainNormalArray"), normalArray);
	material->SetTextureParameterValue(TEXT("TerrainMaskArray"), maskArray);
	material->SetTextureParameterValue(TEXT("TerrainCoverageArray"), coverageArray);
	material->SetTextureParameterValue(TEXT("TerrainTypeTable"), typeTable);
	material->SetTextureParameterValue(TEXT("TerrainLayerTable"), layerTable);
	material->SetTextureParameterValue(TEXT("TerrainNoise"), noise);
	material->SetVectorParameterValue(TEXT("TerrainMapInfo"), FLinearColor((float)mapWidth, (float)mapHeight, (float)typeCount, (float)coverage.sliceCount));
	material->SetVectorParameterValue(TEXT("TerrainWaterEncoding"), FLinearColor(WaterDepthMinMeters, WaterDepthMaxMeters - WaterDepthMinMeters, 0.f, 0.f));

	UE_LOG(LogTerrainMaterial, Log, TEXT("terrain material built: %d types, %d layers (%dx%d, %d mips), coverage %dx%d x%d slices, %.0f ms"),
		typeCount, layerOrder.Num(), res, res, mipCount, coverage.width, coverage.height, coverage.sliceCount, (FPlatformTime::Seconds() - startTime) * 1000.0);
	return material;
}
