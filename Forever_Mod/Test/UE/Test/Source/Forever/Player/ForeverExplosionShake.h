#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"

#include "ForeverExplosionShake.generated.h"

// 爆炸镜头震动的波形：几条不同频率的正弦叠加，振幅按(1-t)^2衰减。UE5.7里ULegacyCameraShake
// 已移除，Perlin/Wave模式在EngineCameras插件里(项目没启用)，所以自己继承UCameraShakePattern。
UCLASS()
class UForeverExplosionShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Shake")
	float duration = 0.45f;

	// 滚转/俯仰/偏航的最大角度(度)。
	UPROPERTY(EditAnywhere, Category = "Shake")
	float pitchAmplitude = 1.2f;
	UPROPERTY(EditAnywhere, Category = "Shake")
	float yawAmplitude = 0.8f;
	UPROPERTY(EditAnywhere, Category = "Shake")
	float rollAmplitude = 1.0f;

	// 位置抖动最大偏移(cm)。
	UPROPERTY(EditAnywhere, Category = "Shake")
	float locationAmplitude = 2.f;

	UPROPERTY(EditAnywhere, Category = "Shake")
	float frequency = 22.f;

private:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;

	float elapsed = 0.f;
	FVector phaseA = FVector::ZeroVector;
	FVector phaseB = FVector::ZeroVector;
};

// 爆炸镜头震动。用UGameplayStatics::PlayWorldCameraShake(世界, 本类, 爆炸中心, 内半径, 外半径)播放，
// 距离衰减由引擎处理。
UCLASS()
class UForeverExplosionShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UForeverExplosionShake(const FObjectInitializer& objectInitializer);
};
