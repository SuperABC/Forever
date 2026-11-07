#include "Framework/ForeverSkyFrameworkComponent.h"

#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
// ASkyAtmosphere这个Actor类本身就声明在Components/SkyAtmosphereComponent.h里
// (没有独立的Engine/SkyAtmosphere.h)，和USkyAtmosphereComponent共用一个头文件。
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Kismet/GameplayStatics.h"

#include "common/utility.h"

UForeverSkyFrameworkComponent::UForeverSkyFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = false;
}

void UForeverSkyFrameworkComponent::EnsureSkyGenerated() {
	UWorld* world = GetWorld();
	if (!world) return;

	if (!sunLight) {
		sunLight = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(world, ADirectionalLight::StaticClass()));
		if (!sunLight) sunLight = world->SpawnActor<ADirectionalLight>();
		if (sunLight) {
			// 坑：代码Spawn出来的DirectionalLight默认Static，运行时改旋转不会重新渲染(太阳纹丝不动)，必须显式Movable。
			if (UDirectionalLightComponent* sunComponent = Cast<UDirectionalLightComponent>(sunLight->GetLightComponent())) {
				sunComponent->SetMobility(EComponentMobility::Movable);
				// 两个DirectionalLight的ForwardShadingPriority默认都是0会触发引擎竞争警告，太阳必须赢，ambientLight保持0。
				sunComponent->SetForwardShadingPriority(1);
			}
		}
	}
	if (!ambientLight) {
		// 不找关卡里现成的，直接Spawn。
		ambientLight = world->SpawnActor<ADirectionalLight>();
		if (ambientLight) {
			if (UDirectionalLightComponent* ambientComponent =
				Cast<UDirectionalLightComponent>(ambientLight->GetLightComponent())) {
				// 同上的Static坑：场景是运行时生成、没烘焙过光照，Static灯光可能根本不生效。
				ambientComponent->SetMobility(EComponentMobility::Movable);
				// 关键：关阴影，光才会穿透墙体/天花板均匀照到所有表面(SkyLight按可见天空算贡献，封闭室内照不到)。
				ambientComponent->SetCastShadows(false);
				// 初始值用白天强度，之后UpdateAmbientLight()每帧按时间插值。
				ambientComponent->SetIntensity(dayAmbientLux);
				ambientComponent->SetLightColor(FLinearColor(0.8f, 0.85f, 1.0f)); // 偏冷调中性光
				// 退出大气/云层的"谁是太阳"竞争，只认sunLight。
				ambientComponent->SetAtmosphereSunLight(false);
			}
			// 朝向无关紧要(无阴影，只影响不同法线朝向面的亮度比例)。
			ambientLight->SetActorRotation(FRotator(-45.0f, 0.0f, 0.0f));
		}
	}
	if (!skyAtmosphere) {
		skyAtmosphere = Cast<ASkyAtmosphere>(UGameplayStatics::GetActorOfClass(world, ASkyAtmosphere::StaticClass()));
		if (!skyAtmosphere) skyAtmosphere = world->SpawnActor<ASkyAtmosphere>();
	}
	if (!skyLight) {
		skyLight = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(world, ASkyLight::StaticClass()));
		if (!skyLight) skyLight = world->SpawnActor<ASkyLight>();
		// 运行时程序化生成的场景要用可移动+实时捕获的SkyLight，它才会跟着SkyAtmosphere变亮变暗；
		// 关卡里手放的SkyLight同样套用(Static的捕获停在编辑器里的状态，夜里会一直漏白天的光)。
		if (skyLight) {
			if (USkyLightComponent* skyComponent = skyLight->GetLightComponent()) {
				skyComponent->SetMobility(EComponentMobility::Movable);
				skyComponent->SetRealTimeCaptureEnabled(true);
			}
		}
	}
	if (!heightFog) {
		heightFog = Cast<AExponentialHeightFog>(UGameplayStatics::GetActorOfClass(world, AExponentialHeightFog::StaticClass()));
		if (!heightFog) heightFog = world->SpawnActor<AExponentialHeightFog>();
		// 默认只走便宜的非体积雾，要光束/自阴影的体积感必须显式开bEnableVolumetricFog。
		if (heightFog) {
			if (UExponentialHeightFogComponent* fogComponent =
				heightFog->GetComponentByClass<UExponentialHeightFogComponent>()) {
				fogComponent->bEnableVolumetricFog = true;
			}
		}
	}
	if (!postProcessVolume) {
		postProcessVolume = Cast<APostProcessVolume>(UGameplayStatics::GetActorOfClass(world, APostProcessVolume::StaticClass()));
		if (!postProcessVolume) postProcessVolume = world->SpawnActor<APostProcessVolume>();
		if (postProcessVolume) {
			// 没有编辑器摆好的包围盒，必须整个世界生效；关卡里手放的也一样处理——夜晚曝光必须全图一致。
			postProcessVolume->bUnbound = true;
			// 优先级比关卡里其它后处理体积高，曝光以这里为准。
			postProcessVolume->Priority = 100.f;
			// override开关程序Spawn/复用都必须显式打开，否则UpdatePostProcess里的赋值无效。
			FPostProcessSettings& settings = postProcessVolume->Settings;
			settings.bOverride_AutoExposureMinBrightness = true;
			settings.bOverride_AutoExposureMaxBrightness = true;
			settings.bOverride_LocalExposureShadowContrastScale = true;
			settings.bOverride_LumenSceneLightingUpdateSpeed = true;
			settings.LumenSceneLightingUpdateSpeed = lumenSceneLightingUpdateSpeed;
			settings.bOverride_LumenFinalGatherLightingUpdateSpeed = true;
			settings.LumenFinalGatherLightingUpdateSpeed = lumenFinalGatherUpdateSpeed;
		}
	}
}

void UForeverSkyFrameworkComponent::UpdateSky(const Time& currentTime) {
	EnsureSkyGenerated();

	float chronode = (currentTime.GetHour() * 3600.f + currentTime.GetMinute() * 60.f +
		currentTime.GetSecond() + currentTime.GetMillisecond() / 1000.f) / (24.f * 3600.f);

	UpdateSunLight(chronode);
	UpdateSkyAtmosphere(chronode);
	UpdateHeightFog(chronode);
	UpdatePostProcess(chronode);
	UpdateAmbientLight(chronode);
}

namespace {
	// 太阳光强度系数(0~1) = relu(-cos(2π·chronode)) = 太阳仰角的正弦：6/18点在地平线，地平线以下恰好为0。
	float SunIntensityFactor(float chronode) {
		return FMath::Max(0.0f, -FMath::Cos(2.0f * PI * chronode));
	}

	constexpr float kSunMaxIntensity = 100000.0f; // 正午直射光，lux

	// 白天系数t(0=深夜,1=白天)：太阳高度正弦在0.03~0.35之间smoothstep。区间必须在日出(sinAlt=0)之后，
	// 否则破晓前曝光/环境光先亮起来；黄昏对称。
	float DayFactor(float chronode) {
		const float sinAlt = -FMath::Cos(2.0f * PI * chronode);
		const float t = FMath::Clamp((sinAlt - 0.03f) / 0.32f, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}
}

void UForeverSkyFrameworkComponent::UpdateSunLight(float chronode) {
	if (!sunLight) return;

	UDirectionalLightComponent* sunComponent = Cast<UDirectionalLightComponent>(sunLight->GetLightComponent());
	if (!sunComponent) return;

	sunLight->SetActorRotation(FRotator(FMath::Lerp(90.0f, -270.0f, chronode), 0.0f, 0.0f));
	sunComponent->SetIntensity(SunIntensityFactor(chronode) * kSunMaxIntensity * weatherLightScale);

	// 颜色关键帧分段插值(HSV)；0.3~0.7正午平台，首尾同为夜间色，跨午夜连续。
	static const TArray<TPair<float, FLinearColor>> colorKeyframes = {
		{0.00f, FLinearColor(0.5f, 0.5f, 0.8f)},  // 夜间
		{0.15f, FLinearColor(1.0f, 0.5f, 0.2f)},  // 凌晨/清晨朝霞
		{0.25f, FLinearColor(1.0f, 0.7f, 0.3f)},  // 清晨过渡
		{0.30f, FLinearColor(1.0f, 0.95f, 0.9f)}, // 正午平台起点
		{0.70f, FLinearColor(1.0f, 0.95f, 0.9f)}, // 正午平台终点
		{0.75f, FLinearColor(1.0f, 0.5f, 0.2f)},  // 黄昏夕阳(与清晨朝霞对称)
		{0.85f, FLinearColor(0.5f, 0.5f, 0.8f)},  // 入夜
		{1.00f, FLinearColor(0.5f, 0.5f, 0.8f)},  // 夜间(=0.00，闭合整个环)
	};
	FLinearColor lightColor = colorKeyframes.Last().Value;
	for (int32 i = 0; i < colorKeyframes.Num() - 1; i++) {
		const auto& cur = colorKeyframes[i];
		const auto& next = colorKeyframes[i + 1];
		if (chronode >= cur.Key && chronode <= next.Key) {
			float segLen = next.Key - cur.Key;
			float t = segLen > 0.0f ? (chronode - cur.Key) / segLen : 0.0f;
			lightColor = FLinearColor::LerpUsingHSV(cur.Value, next.Value, t);
			break;
		}
	}
	sunComponent->SetLightColor(lightColor);
}

void UForeverSkyFrameworkComponent::UpdateSkyAtmosphere(float chronode) {
	if (!skyAtmosphere) return;

	USkyAtmosphereComponent* atmosphereComponent = skyAtmosphere->GetComponent();
	if (atmosphereComponent) {
		float sunIntensity = SunIntensityFactor(chronode) * kSunMaxIntensity * weatherLightScale;
		atmosphereComponent->SetMultiScatteringFactor(FMath::Clamp(sunIntensity / 50000.0f, 0.1f, 1.0f));
	}
}

void UForeverSkyFrameworkComponent::UpdateHeightFog(float chronode) {
	if (!heightFog) return;

	UExponentialHeightFogComponent* fogComponent =
		heightFog->GetComponentByClass<UExponentialHeightFogComponent>();
	if (fogComponent) {
		// 夜间雾比白天浓：午夜0.08，正午0.05，中间按(1-|chronode-0.5|*2)^2过渡。
		float fogDensity = 0.08f - FMath::Pow(1.0f - FMath::Abs(chronode - 0.5f) * 2.0f, 2.0f) * 0.03f;
		fogComponent->SetFogDensity(fogDensity * weatherFogScale);
	}
}

void UForeverSkyFrameworkComponent::UpdatePostProcess(float chronode) {
	if (!postProcessVolume) return;

	const float t = DayFactor(chronode);
	FPostProcessSettings& settings = postProcessVolume->Settings;
	settings.AutoExposureMinBrightness = FMath::Lerp(nightExposureEV, dayExposureMinEV, t);
	settings.AutoExposureMaxBrightness = dayExposureMaxEV;
	settings.LocalExposureShadowContrastScale = FMath::Lerp(nightLocalShadowContrast, dayLocalShadowContrast, t);
	currentExposureEV = FMath::Lerp(nightExposureEV, referenceDayEV, t);
}

void UForeverSkyFrameworkComponent::UpdateAmbientLight(float chronode) {
	if (!ambientLight) return;

	if (UDirectionalLightComponent* ambientComponent = Cast<UDirectionalLightComponent>(ambientLight->GetLightComponent())) {
		// 天气只压一半：阴天的室内/背光处不至于比晴天暗得太多。
		ambientComponent->SetIntensity(FMath::Lerp(nightAmbientLux, dayAmbientLux, DayFactor(chronode)) * FMath::Lerp(1.f, weatherLightScale, 0.5f));
	}
}
