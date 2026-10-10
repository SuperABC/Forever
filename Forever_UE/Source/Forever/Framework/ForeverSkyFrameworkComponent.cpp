#include "Framework/ForeverSkyFrameworkComponent.h"

#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
// ASkyAtmosphere这个Actor类本身就声明在Components/SkyAtmosphereComponent.h里
// (没有独立的Engine/SkyAtmosphere.h)，和USkyAtmosphereComponent共用一个头文件。
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Kismet/GameplayStatics.h"

#include "common/utility.h"

UForeverSkyFrameworkComponent::UForeverSkyFrameworkComponent() {
	// 不开自己的Tick——由AForeverFrameworkActor::Tick()统一调用UpdateSky，见类头注释。
	PrimaryComponentTick.bCanEverTick = false;
}

void UForeverSkyFrameworkComponent::EnsureSkyGenerated() {
	UWorld* world = GetWorld();
	if (!world) return;

	if (!sunLight) {
		sunLight = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(world, ADirectionalLight::StaticClass()));
		if (!sunLight) sunLight = world->SpawnActor<ADirectionalLight>();
		if (sunLight) {
			// 原生C++类默认构造的UDirectionalLightComponent，Mobility默认是Static(不是
			// 编辑器"放置Actor"面板那个默认Movable的蓝图模板)——Static灯光的方向在运行时
			// 改了不会重新渲染，UpdateSunLight()里的SetActorRotation会执行成功但太阳视觉上
			// 完全不动，这是实测确认的根因。必须显式设成Movable才能让每帧的旋转真正生效。
			if (UDirectionalLightComponent* sunComponent = Cast<UDirectionalLightComponent>(sunLight->GetLightComponent())) {
				sunComponent->SetMobility(EComponentMobility::Movable);
			}
		}
	}
	if (!skyAtmosphere) {
		skyAtmosphere = Cast<ASkyAtmosphere>(UGameplayStatics::GetActorOfClass(world, ASkyAtmosphere::StaticClass()));
		if (!skyAtmosphere) skyAtmosphere = world->SpawnActor<ASkyAtmosphere>();
	}
	if (!skyLight) {
		skyLight = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(world, ASkyLight::StaticClass()));
		if (!skyLight) skyLight = world->SpawnActor<ASkyLight>();
	}
	if (!heightFog) {
		heightFog = Cast<AExponentialHeightFog>(UGameplayStatics::GetActorOfClass(world, AExponentialHeightFog::StaticClass()));
		if (!heightFog) heightFog = world->SpawnActor<AExponentialHeightFog>();
	}
	if (!postProcessVolume) {
		postProcessVolume = Cast<APostProcessVolume>(UGameplayStatics::GetActorOfClass(world, APostProcessVolume::StaticClass()));
		if (!postProcessVolume) {
			postProcessVolume = world->SpawnActor<APostProcessVolume>();
			if (postProcessVolume) {
				// 没有编辑器摆好的包围盒，必须整个世界生效。
				postProcessVolume->bUnbound = true;
				// 老工程这个override开关是在编辑器Details面板手动勾的，这次程序Spawn必须
				// 显式打开，否则下面UpdatePostProcess()对AutoExposureMinBrightness的赋值
				// 不会有任何效果。
					postProcessVolume->Settings.bOverride_AutoExposureMinBrightness = true;
			}
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
}

void UForeverSkyFrameworkComponent::UpdateSunLight(float chronode) {
	if (!sunLight) return;

	UDirectionalLightComponent* sunComponent = Cast<UDirectionalLightComponent>(sunLight->GetLightComponent());
	if (!sunComponent) return;

	sunLight->SetActorRotation(FRotator(FMath::Lerp(90.0f, -270.0f, chronode), 0.0f, 0.0f));
	sunComponent->SetIntensity(FMath::Clamp(FMath::Sin(chronode * PI), 0.0f, 1.0f) * 100000.0f);

	FLinearColor lightColor;
	// 凌晨至清晨（0~0.2）：橙红色朝霞，太阳刚升起时的低角度暖色调
	if (chronode < 0.2f)
		lightColor = FLinearColor(1.0f, 0.5f, 0.2f);
	// 清晨过渡段（0.2~0.3）：橙黄色，日出后光线逐渐变暖变亮
	else if (chronode < 0.3f)
		lightColor = FLinearColor(1.0f, 0.7f, 0.3f);
	// 白天正午段（0.3~0.7）：接近白色的暖白光，模拟正午日光
	else if (chronode < 0.7f)
		lightColor = FLinearColor(1.0f, 0.95f, 0.9f);
	// 黄昏过渡段（0.7~0.8）：橙红色夕阳，与清晨对称的低角度暖色调
	else if (chronode < 0.8f)
		lightColor = FLinearColor(1.0f, 0.5f, 0.2f);
	// 夜间（0.8~1.0）：偏蓝紫色的月光/环境光，模拟夜间低照度冷色调
	else
		lightColor = FLinearColor(0.5f, 0.5f, 0.8f);
	sunComponent->SetLightColor(lightColor);
}

void UForeverSkyFrameworkComponent::UpdateSkyAtmosphere(float chronode) {
	if (!skyAtmosphere) return;

	USkyAtmosphereComponent* atmosphereComponent = skyAtmosphere->GetComponent();
	if (atmosphereComponent) {
		float sunIntensity = FMath::Clamp(FMath::Sin(chronode * PI), 0.0f, 1.0f) * 100000.0f;
		atmosphereComponent->SetMultiScatteringFactor(FMath::Clamp(sunIntensity / 50000.0f, 0.1f, 1.0f));
	}
}

void UForeverSkyFrameworkComponent::UpdateHeightFog(float chronode) {
	if (!heightFog) return;

	UExponentialHeightFogComponent* fogComponent =
		heightFog->GetComponentByClass<UExponentialHeightFogComponent>();
	if (fogComponent) {
		float fogDensity = 0.0001f + FMath::Pow(1.0f - FMath::Abs(chronode - 0.5f) * 2.0f, 2.0f) * 0.0001f;
		fogComponent->SetFogDensity(fogDensity);
	}
}

void UForeverSkyFrameworkComponent::UpdatePostProcess(float chronode) {
	if (!postProcessVolume) return;

	float globalIntensity = -FMath::Cos(chronode * PI * 2);
	postProcessVolume->Settings.AutoExposureMinBrightness = globalIntensity * 10;
}
