#include "Framework/ForeverWaterFrameworkComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/PostProcessVolume.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "ProceduralMeshComponent.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverSkyFrameworkComponent.h"

#include "Water/WaterQuery.h"
#include "map/map.h"

using namespace std;

#define WATER_COLLECTION_PATH "/Game/Asset/Water/MPC_Water.MPC_Water"
#define WATER_UNDERWATER_PATH "/Game/Asset/Water/PP_Underwater.PP_Underwater"
#define WATER_PLUGIN_MATERIAL_PATH "/Water/Materials/WaterSurface/Water_Material_CustomMesh.Water_Material_CustomMesh"

namespace {
	constexpr float kWorldScale = 1000.f; // 地图1个元素=1000cm
	constexpr float kHideSurfaceDepth = 0.f; // 相机低于水面这么多cm就隐藏水面mesh(0=一低于水面就隐藏：水面mesh紧贴相机时会进场景深度，后处理判不出视线穿出水面，画面会闪、露出倒影)
}

UForeverWaterFrameworkComponent::UForeverWaterFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = true;
	// 放到相机更新之后再读相机位置：否则读到的是上一帧的相机，入水/出水的判断会晚一帧
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

// 水面mesh(还原自原来的地形组件BuildOceanMesh)：整张图一个共享顶点网格，有水的格子把4个角写成各自的水位
// (相邻格水位不同时共享顶点被后写的覆盖)，河流水面会倾斜所以按面法线面积加权累加成顶点法线。
void UForeverWaterFrameworkComponent::GenerateWater(Map* inMap) {
	map = inMap;
	if (!map) return;
	AActor* owner = GetOwner();
	if (!owner) return;

	waterCollection = LoadObject<UMaterialParameterCollection>(nullptr, TEXT(WATER_COLLECTION_PATH));
	underwaterMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT(WATER_UNDERWATER_PATH));

	auto [width, height] = map->GetSize();

	TArray<FVector> vertices;
	TArray<FVector2D> uvs;
	vertices.Reserve((width + 1) * (height + 1));
	uvs.Reserve((width + 1) * (height + 1));
	for (int vy = 0; vy <= height; vy++)
		for (int vx = 0; vx <= width; vx++) {
			vertices.Add(FVector(vx * kWorldScale, vy * kWorldScale, 0.f));
			uvs.Add(FVector2D(vx, vy));
		}

	TArray<int32> triangles;
	auto vertIdx = [width](int vx, int vy) { return vy * (width + 1) + vx; };
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			auto [water, h] = map->GetWater(x, y);
			if (!water) continue;

			int idxBL = vertIdx(x, y), idxBR = vertIdx(x + 1, y);
			int idxTL = vertIdx(x, y + 1), idxTR = vertIdx(x + 1, y + 1);
			vertices[idxBL].Z = h * kWorldScale;
			vertices[idxBR].Z = h * kWorldScale;
			vertices[idxTL].Z = h * kWorldScale;
			vertices[idxTR].Z = h * kWorldScale;
			triangles.Add(idxBL); triangles.Add(idxTL); triangles.Add(idxTR);
			triangles.Add(idxBL); triangles.Add(idxTR); triangles.Add(idxBR);
		}
	}
	if (triangles.Num() == 0) return;

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
		oceanMesh->SetCastShadow(false);
		oceanMesh->RegisterComponent();
		owner->AddInstanceComponent(oceanMesh);
	}
	oceanMesh->CreateMeshSection(0, vertices, triangles, normals, uvs, TArray<FColor>(), tangents, false);

	UMaterialInterface* base = LoadObject<UMaterialInterface>(nullptr, TEXT(WATER_PLUGIN_MATERIAL_PATH));
	if (!oceanMaterial && base) oceanMaterial = UMaterialInstanceDynamic::Create(base, this);
	if (oceanMaterial) oceanMesh->SetMaterial(0, oceanMaterial);
}

void UForeverWaterFrameworkComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!map) return;
	if (const AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(GetOwner())) {
		if (!frameworkActor->IsInitialGenerationDone()) return;
	}
	UpdateWaterParameters();
}

void UForeverWaterFrameworkComponent::UpdateWaterParameters() {
	UWorld* world = GetWorld();
	if (!world) return;

	// 相机所在格有没有水：决定水下后处理开不开，也给后处理材质提供水面基准高度(只用公式算，不管像素落在哪个格)。
	float baseLevel = 0.f;
	bool cameraInWaterCell = false;
	bool cameraDeepUnderwater = false;
	if (APlayerCameraManager* cameraManager = UGameplayStatics::GetPlayerCameraManager(world, 0)) {
		const FVector cameraLocation = cameraManager->GetCameraLocation();
		const auto mapSize = map->GetSize();
		const int cellX = FMath::FloorToInt(cameraLocation.X / kWorldScale);
		const int cellY = FMath::FloorToInt(cameraLocation.Y / kWorldScale);
		if (cellX >= 0 && cellY >= 0 && cellX < mapSize.first && cellY < mapSize.second
				&& !ForeverWater::IsBelowTerrain(map, cameraLocation)) { // 相机在地形表面以下(海底隧道里)不算在水里：不开水下后处理，也不隐藏水面
			const auto [hasWater, level] = map->GetWater(cellX, cellY);
			if (hasWater) {
				baseLevel = level * kWorldScale;
				cameraInWaterCell = true;
				cameraDeepUnderwater = cameraLocation.Z < baseLevel - kHideSurfaceDepth;
			}
		}
	}

	// 相机低于水面时隐藏水面mesh：水下看水面由PP_Underwater按菲涅尔自己画，不要mesh自带的SSR反射。
	// 隐藏用"相对缩放缩到1e-6"而不是SetVisibility：SetVisibility会MarkRenderStateDirty重建这张整图大小的mesh的渲染代理，
	// 相机在水面附近上下来回时每次切换都卡一下(帧率暴跌)，而且重建要跨帧，隐藏会比相机入水晚；改缩放只更新变换，当帧生效。
	if (oceanMesh && cameraDeepUnderwater != bOceanHidden) {
		bOceanHidden = cameraDeepUnderwater;
		oceanMesh->SetRelativeScale3D(bOceanHidden ? FVector(1e-6) : FVector::OneVector);
	}

	if (waterCollection) {
		if (UMaterialParameterCollectionInstance* instance = world->GetParameterCollectionInstance(waterCollection)) {
			bool ok = instance->SetScalarParameterValue(TEXT("WaterTime"), world->GetTimeSeconds());
			// 水下雾亮度的基准(场景辐亮度，单位同光照度)：阳光照亮的白色漫反射面的辐亮度≈照度*0.5/π。雾色(显示比例c)乘它，
			// 雾就和"阳光下的海底"保持固定亮度比，和曝光无关；夜里照度低，水下自然变暗。后处理里再乘PreExposure换到预曝光空间。
			const AForeverFrameworkActor* lightActor = Cast<AForeverFrameworkActor>(GetOwner());
			const UForeverSkyFrameworkComponent* lightSky = lightActor ? lightActor->GetSkyFramework() : nullptr;
			const float referenceRadiance = lightSky ? lightSky->GetCurrentLightLux() * 0.5f / UE_PI : 10000.f;
			ok &= instance->SetScalarParameterValue(TEXT("WaterLightScale"), referenceRadiance);
			// 相机所在格没水时把水面基准高度设成极低(-1e9)：后处理判据"视线起点低于水面"永远不成立，
			// 不会拿着上一次进过的水格的旧水位，在没水格的地下区域也画出水下效果(权重极小的预热阶段同理)。
			ok &= instance->SetScalarParameterValue(TEXT("WaterBaseLevel"), cameraInWaterCell ? baseLevel : -1.0e9f);
			if (!ok && !bCollectionWarned) {
				bCollectionWarned = true;
				UE_LOG(LogTemp, Warning, TEXT("UForeverWaterFrameworkComponent: MPC_Water lacks WaterTime/WaterBaseLevel/WaterLightScale (rerun Tools/WaterMaterial/build_water_assets.py)."));
			}
		}
	}

	// 水下后处理：挂在天空组件的PostProcessVolume上，权重0/1，权重0时引擎整个跳过这个后处理。
	if (underwaterMaterial) {
		const AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(GetOwner());
		UForeverSkyFrameworkComponent* sky = frameworkActor ? frameworkActor->GetSkyFramework() : nullptr;
		APostProcessVolume* volume = sky ? sky->GetPostProcessVolume() : nullptr;
		if (volume) {
			FPostProcessSettings& settings = volume->Settings;
			if (!bUnderwaterBlendableAdded) {
				settings.AddBlendable(underwaterMaterial, 0.f);
				bUnderwaterBlendableAdded = true;
			}
			// 权重0时引擎整个跳过这个后处理，它的shader也就一直没被请求编译：第一次进水才开始编译，那几帧画面会异常(默认后处理)。
			// 所以没编译完之前用一个极小的权重让它每帧"在用"，提前触发编译；编译完就回到0，没进水时不产生开销。
			if (!bUnderwaterShaderReady) {
#if WITH_EDITOR
				// IsCompilationFinished是编辑器专用API(打包版编译不过)；打包版shader是烘焙好的，没有"等编译"这回事。
				const FMaterialResource* resource = underwaterMaterial->GetMaterialResource(world->GetFeatureLevel());
				if (resource && resource->IsCompilationFinished()) bUnderwaterShaderReady = true;
#else
				bUnderwaterShaderReady = true;
#endif
			}
			const float weight = cameraInWaterCell ? 1.f : (bUnderwaterShaderReady ? 0.f : 0.001f);
			if (weight != underwaterWeight) {
				for (FWeightedBlendable& blendable : settings.WeightedBlendables.Array) {
					if (blendable.Object == underwaterMaterial) blendable.Weight = weight;
				}
				underwaterWeight = weight;
			}
		}
	}
}
