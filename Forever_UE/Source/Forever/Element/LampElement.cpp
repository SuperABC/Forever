#include "Element/LampElement.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"

#include "Element/LampDecalComponent.h"

ALampElement::ALampElement() {
	// 路灯不需要Tick，亮度/近远只在变化时由UForeverLampFrameworkComponent推送。
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(root);
}

void ALampElement::BeginPlay() {
	Super::BeginPlay();
	// 初始状态(远、灭)：让蓝图里加的Decal一开始就隐藏。
	RefreshLampEffects();
}

void ALampElement::ApplyBrightness(float newBrightness) {
	brightness = newBrightness;
	SetBrightness(newBrightness);
	RefreshLampEffects();
}

void ALampElement::SwitchNear_Implementation() {
	bNear = true;
	RefreshLampEffects();
}

void ALampElement::SwitchFar_Implementation() {
	bNear = false;
	RefreshLampEffects();
}

void ALampElement::RefreshLampEffects() {
	bool lit = brightness > 0.f;

	// 真实光源和Decal二选一：近处亮灯=真实光源，远处亮灯=Decal，灭了两者都没有。
	if (bNear && lit) {
		CreateRealLights();
	}
	else {
		DestroyRealLights();
	}

	TInlineComponentArray<ULampDecalComponent*> decals;
	GetComponents(decals);
	for (ULampDecalComponent* decal : decals) {
		decal->SetLampActive(!bNear && lit, brightness);
	}
}

void ALampElement::CreateRealLights() {
	if (activeLights.Num() > 0) return; // 已经创建过，不重复

	for (const FLampLightSettings& settings : lights) {
		ULightComponent* light = nullptr;
		if (settings.type == ELampLightType::Spot) {
			USpotLightComponent* spot = NewObject<USpotLightComponent>(this);
			spot->SetInnerConeAngle(settings.innerConeAngle);
			spot->SetOuterConeAngle(settings.outerConeAngle);
			light = spot;
		}
		else {
			light = NewObject<UPointLightComponent>(this);
		}
		if (ULocalLightComponent* local = Cast<ULocalLightComponent>(light)) {
			local->SetIntensityUnits(ELightUnits::Candelas);
			local->SetAttenuationRadius(settings.attenuationRadius);
		}
		light->SetMobility(EComponentMobility::Movable);
		light->SetLightColor(settings.color);
		light->SetIntensity(settings.intensity);
		light->SetCastShadows(settings.castShadows);
		light->SetupAttachment(GetRootComponent());
		light->SetRelativeLocationAndRotation(settings.relativeLocation, settings.relativeRotation);
		light->RegisterComponent();
		activeLights.Add(light);
	}
}

void ALampElement::DestroyRealLights() {
	for (ULightComponent* light : activeLights) {
		if (light) light->DestroyComponent();
	}
	activeLights.Reset();
}
