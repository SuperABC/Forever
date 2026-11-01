#include "Framework/ForeverWeatherFrameworkComponent.h"

#include "Components/DecalComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/PlayerCameraManager.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverSkyFrameworkComponent.h"
#include "Framework/ForeverVegetationFrameworkComponent.h"

#include "player/player.h"
#include "player/weather.h"

// 引擎自带的简单体积云材质实例，参数见.md。
#define WEATHER_CLOUD_MATERIAL_PATH "/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"
// 雨/雪的Niagara系统，需要在编辑器里做，暴露User.Intensity(float)和User.Wind(vector3)，见.md。
#define WEATHER_RAIN_SYSTEM_PATH "/Game/Asset/Effects/Weather/NS_Rain.NS_Rain"
#define WEATHER_SNOW_SYSTEM_PATH "/Game/Asset/Effects/Weather/NS_Snow.NS_Snow"
// 地面湿润/积雪贴花材质(Deferred Decal)，需要在编辑器里做，标量参数Wetness/SnowCover(0~1)，见.md。
#define WEATHER_GROUND_DECAL_PATH "/Game/Asset/Effects/Weather/M_WeatherGround.M_WeatherGround"

UForeverWeatherFrameworkComponent::UForeverWeatherFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = false;

	// 下标=Weather::Type：晴/多云/阴/小雨/大雨/小雪/大雪。数值是第一版粗估，PIE里调。
	auto make = [](float coverage, float density, float lightScale, float fogScale,
		float windStrength, float windSpeed, float rain, float snow) {
		FForeverWeatherParams params;
		params.cloudCoverage = coverage;
		params.cloudDensity = density;
		params.lightScale = lightScale;
		params.fogScale = fogScale;
		params.windStrengthScale = windStrength;
		params.windSpeedScale = windSpeed;
		params.rain = rain;
		params.snow = snow;
		return params;
	};
	weatherParams = {
		make(0.15f, 0.6f, 1.00f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f), // 晴：微风，没有天气效果，薄雾(基准，不能没有)
		make(0.55f, 0.8f, 1.00f, 1.2f, 1.0f, 1.0f, 0.0f, 0.0f), // 多云：云变密，阳光不受影响，薄雾
		make(1.00f, 1.0f, 0.35f, 2.5f, 1.6f, 1.2f, 0.0f, 0.0f), // 阴：云盖满天，阳光变暗，风大一点，中雾
		make(1.00f, 1.0f, 0.35f, 2.5f, 1.6f, 1.2f, 0.4f, 0.0f), // 小雨：阴+雨，中雾
		make(1.00f, 1.0f, 0.18f, 5.0f, 3.0f, 1.5f, 1.0f, 0.0f), // 大雨：阴+更多雨+大风，大雾
		make(0.55f, 0.8f, 1.00f, 2.5f, 1.0f, 1.0f, 0.0f, 0.4f), // 小雪：多云+雪，中雾
		make(0.55f, 0.8f, 0.30f, 5.0f, 3.0f, 1.5f, 0.0f, 1.0f), // 大雪：多云+更多雪+大风，大雾
	};
}

void UForeverWeatherFrameworkComponent::EnsureWeatherGenerated() {
	if (bGenerated) return;
	AActor* owner = GetOwner();
	UWorld* world = GetWorld();
	if (!owner || !world) return;
	bGenerated = true;

	skyFramework = owner->FindComponentByClass<UForeverSkyFrameworkComponent>();
	vegetationFramework = owner->FindComponentByClass<UForeverVegetationFrameworkComponent>();

	// 体积云：关卡里已经有就复用，没有就Spawn一个(和天空组件同一个"Ensure"模式)。
	volumetricCloud = Cast<AVolumetricCloud>(UGameplayStatics::GetActorOfClass(world, AVolumetricCloud::StaticClass()));
	if (!volumetricCloud) volumetricCloud = world->SpawnActor<AVolumetricCloud>();
	if (volumetricCloud) {
		UVolumetricCloudComponent* cloudComponent = volumetricCloud->GetComponentByClass<UVolumetricCloudComponent>();
		UMaterialInterface* baseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT(WEATHER_CLOUD_MATERIAL_PATH));
		if (cloudComponent && baseMaterial) {
			cloudMaterial = UMaterialInstanceDynamic::Create(baseMaterial, this);
			cloudComponent->SetMaterial(cloudMaterial);
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("UForeverWeatherFrameworkComponent: cloud component or material %s is unavailable."), TEXT(WEATHER_CLOUD_MATERIAL_PATH));
		}
	}

	rainComponent = CreatePrecipitation(TEXT(WEATHER_RAIN_SYSTEM_PATH));
	snowComponent = CreatePrecipitation(TEXT(WEATHER_SNOW_SYSTEM_PATH));

	// 地面贴花：朝下投影(Decal沿本地X轴投影，pitch=-90让X指向地面)，先隐藏，有湿润/积雪才显示。
	UMaterialInterface* decalBase = LoadObject<UMaterialInterface>(nullptr, TEXT(WEATHER_GROUND_DECAL_PATH));
	if (decalBase) {
		groundMaterial = UMaterialInstanceDynamic::Create(decalBase, this);
		groundDecal = NewObject<UDecalComponent>(owner, NAME_None, RF_Transient);
		groundDecal->SetDecalMaterial(groundMaterial);
		groundDecal->DecalSize = FVector(decalHalfHeight, decalRadius, decalRadius);
		groundDecal->SetupAttachment(owner->GetRootComponent());
		groundDecal->SetVisibility(false);
		groundDecal->RegisterComponent();
		owner->AddInstanceComponent(groundDecal);
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("UForeverWeatherFrameworkComponent: ground decal material %s not found, wet/snow ground is skipped."), TEXT(WEATHER_GROUND_DECAL_PATH));
	}
}

UNiagaraComponent* UForeverWeatherFrameworkComponent::CreatePrecipitation(const TCHAR* assetPath) {
	AActor* owner = GetOwner();
	UNiagaraSystem* system = LoadObject<UNiagaraSystem>(nullptr, assetPath);
	if (!owner || !system) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverWeatherFrameworkComponent: Niagara system %s not found, that precipitation is skipped."), assetPath);
		return nullptr;
	}

	UNiagaraComponent* component = NewObject<UNiagaraComponent>(owner, NAME_None, RF_Transient);
	component->SetAsset(system);
	component->SetAutoActivate(false);
	component->SetupAttachment(owner->GetRootComponent());
	component->RegisterComponent();
	owner->AddInstanceComponent(component);
	return component;
}

void UForeverWeatherFrameworkComponent::UpdateWeather(float deltaTime, const Weather& weather, bool playerIndoors) {
	EnsureWeatherGenerated();

	int32 index = static_cast<int32>(weather.GetType());
	if (!weatherParams.IsValidIndex(index)) return;
	const FForeverWeatherParams& target = weatherParams[index];

	if (!bInitialized) {
		// 第一帧(新开局/读档)直接到位，不从晴天慢慢过渡。
		current = target;
		bInitialized = true;
	}
	else {
		// 指数趋近：transitionSeconds内走完约95%(3个时间常数)。
		float alpha = 1.f - FMath::Exp(-3.f * deltaTime / FMath::Max(transitionSeconds, 0.01f));
		current.cloudCoverage = FMath::Lerp(current.cloudCoverage, target.cloudCoverage, alpha);
		current.cloudDensity = FMath::Lerp(current.cloudDensity, target.cloudDensity, alpha);
		current.lightScale = FMath::Lerp(current.lightScale, target.lightScale, alpha);
		current.fogScale = FMath::Lerp(current.fogScale, target.fogScale, alpha);
		current.windStrengthScale = FMath::Lerp(current.windStrengthScale, target.windStrengthScale, alpha);
		current.windSpeedScale = FMath::Lerp(current.windSpeedScale, target.windSpeedScale, alpha);
		current.rain = FMath::Lerp(current.rain, target.rain, alpha);
		current.snow = FMath::Lerp(current.snow, target.snow, alpha);
	}

	if (skyFramework) skyFramework->SetWeatherScales(current.lightScale, current.fogScale);
	if (vegetationFramework) vegetationFramework->SetWindScale(current.windStrengthScale, current.windSpeedScale);

	UpdateClouds(current);
	UpdatePrecipitation(deltaTime, current, playerIndoors);
	UpdateGround(deltaTime, current);
}

void UForeverWeatherFrameworkComponent::UpdateGround(float deltaTime, const FForeverWeatherParams& params) {
	// 湿润度/积雪度向降水强度靠拢：强度更高时积累快，更低时消退慢(雨停后地面还湿一阵)。
	auto approach = [&](float value, float target, float riseSeconds, float fallSeconds) {
		float rate = 1.f / FMath::Max(target > value ? riseSeconds : fallSeconds, 0.01f);
		return FMath::FInterpConstantTo(value, target, deltaTime, rate);
	};
	wetness = approach(wetness, FMath::Clamp(params.rain, 0.f, 1.f), wetSeconds, drySeconds);
	snowCover = approach(snowCover, FMath::Clamp(params.snow, 0.f, 1.f), snowSeconds, meltSeconds);

	if (!groundDecal || !groundMaterial) return;
	const bool wantVisible = wetness > 0.01f || snowCover > 0.01f;
	if (groundDecal->IsVisible() != wantVisible) groundDecal->SetVisibility(wantVisible);
	if (!wantVisible) return;

	UWorld* world = GetWorld();
	APlayerCameraManager* camera = world ? UGameplayStatics::GetPlayerCameraManager(world, 0) : nullptr;
	if (camera) {
		// 贴一个水平格子对齐，镜头小幅移动时贴花不用每帧重投影(格子=半径/100)。
		const double grid = FMath::Max(decalRadius / 100.0, 1.0);
		FVector location = camera->GetCameraLocation();
		location.X = FMath::GridSnap(location.X, grid);
		location.Y = FMath::GridSnap(location.Y, grid);
		groundDecal->SetWorldLocationAndRotation(location, FRotator(-90.0, 0.0, 0.0));
	}
	groundMaterial->SetScalarParameterValue(TEXT("Wetness"), wetness);
	groundMaterial->SetScalarParameterValue(TEXT("SnowCover"), snowCover);
}

void UForeverWeatherFrameworkComponent::UpdateClouds(const FForeverWeatherParams& params) {
	if (!cloudMaterial) return;

	float unused = 0.f;
	bool hasCoverage = cloudMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(cloudCoverageParameter), unused);
	bool hasDensity = cloudMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(cloudDensityParameter), unused);
	if (!hasCoverage || !hasDensity) {
		// 参数名对不上：列出这个材质实际有的标量参数，方便改cloudCoverageParameter/cloudDensityParameter。
		if (!bCloudParameterWarned) {
			bCloudParameterWarned = true;
			TArray<FMaterialParameterInfo> infos;
			TArray<FGuid> ids;
			cloudMaterial->GetAllScalarParameterInfo(infos, ids);
			FString names;
			for (const FMaterialParameterInfo& info : infos) names += info.Name.ToString() + TEXT(" ");
			UE_LOG(LogTemp, Warning, TEXT("UForeverWeatherFrameworkComponent: cloud material lacks %s / %s. Scalar parameters available: %s"),
				*cloudCoverageParameter.ToString(), *cloudDensityParameter.ToString(), *names);
		}
		if (!hasCoverage && !hasDensity) return;
	}
	if (hasCoverage) cloudMaterial->SetScalarParameterValue(cloudCoverageParameter, params.cloudCoverage);
	if (hasDensity) cloudMaterial->SetScalarParameterValue(cloudDensityParameter, params.cloudDensity);
}

void UForeverWeatherFrameworkComponent::UpdatePrecipitation(float deltaTime, const FForeverWeatherParams& params, bool playerIndoors) {
	// 室内系数：玩家在室内趋近0，在室外趋近1，出门/进门雨雪平滑出现/消失。
	outdoorFactor = FMath::FInterpConstantTo(outdoorFactor, playerIndoors ? 0.f : 1.f, deltaTime, 1.f / FMath::Max(indoorFadeSeconds, 0.01f));

	UWorld* world = GetWorld();
	APlayerCameraManager* camera = world ? UGameplayStatics::GetPlayerCameraManager(world, 0) : nullptr;
	FVector cameraLocation = camera ? camera->GetCameraLocation() : FVector::ZeroVector;

	// 风向借用树的风向，风速随风力倍数变化。
	FVector windDirection = vegetationFramework ? vegetationFramework->GetWindDirection() : FVector(1.0, 0.4, 0.0);
	FVector wind = windDirection * precipitationWindSpeed * params.windStrengthScale;

	auto drive = [&](UNiagaraComponent* component, float intensity) {
		if (!component) return;
		intensity *= outdoorFactor;
		const bool wantActive = intensity > 0.01f;
		if (wantActive && !component->IsActive()) component->Activate(true);
		else if (!wantActive && component->IsActive()) component->Deactivate();
		if (!wantActive) return;
		component->SetWorldLocation(cameraLocation);
		component->SetVariableFloat(TEXT("User.Intensity"), intensity);
		component->SetVariableVec3(TEXT("User.Wind"), wind);
	};
	drive(rainComponent, params.rain);
	drive(snowComponent, params.snow);
}

namespace {
	// Forever.SetWeather <name>：PIE里测试用，走和脚本change_weather同一条路(Weather::SetByName)。
	FAutoConsoleCommandWithWorldAndArgs GSetWeatherCommand(
		TEXT("Forever.SetWeather"),
		TEXT("Force the weather: sunny cloudy overcast light_rain heavy_rain light_snow heavy_snow"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world) {
			if (!world) return;
			for (TActorIterator<AForeverFrameworkActor> it(world); it; ++it) {
				Player* player = it->GetPlayer();
				if (!player) continue;
				if (args.Num() == 0 || !player->GetWeather().SetByName(TCHAR_TO_UTF8(*args[0]))) {
					UE_LOG(LogTemp, Warning, TEXT("Forever.SetWeather: expected one of sunny cloudy overcast light_rain heavy_rain light_snow heavy_snow"));
				}
				return;
			}
		}));
}
