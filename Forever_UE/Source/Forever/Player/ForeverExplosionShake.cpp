#include "Player/ForeverExplosionShake.h"

void UForeverExplosionShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(duration);
	OutInfo.BlendIn = 0.f;
	OutInfo.BlendOut = 0.1f;
}

void UForeverExplosionShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	elapsed = 0.f;
	// 每次随机相位，连续几次爆炸不会抖得一模一样。
	phaseA = FVector(FMath::FRandRange(0.f, TWO_PI), FMath::FRandRange(0.f, TWO_PI), FMath::FRandRange(0.f, TWO_PI));
	phaseB = FVector(FMath::FRandRange(0.f, TWO_PI), FMath::FRandRange(0.f, TWO_PI), FMath::FRandRange(0.f, TWO_PI));
}

void UForeverExplosionShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	elapsed += Params.DeltaTime;
	float t = FMath::Clamp(elapsed / duration, 0.f, 1.f);
	float envelope = FMath::Square(1.f - t);
	float w = TWO_PI * frequency * elapsed;

	OutResult.Rotation = FRotator(
		pitchAmplitude * envelope * FMath::Sin(w + phaseA.X),
		yawAmplitude * envelope * FMath::Sin(w * 0.8f + phaseA.Y),
		rollAmplitude * envelope * FMath::Sin(w * 1.3f + phaseA.Z));
	OutResult.Location = FVector(
		locationAmplitude * envelope * FMath::Sin(w * 1.1f + phaseB.X),
		locationAmplitude * envelope * FMath::Sin(w * 0.9f + phaseB.Y),
		locationAmplitude * envelope * FMath::Sin(w * 1.4f + phaseB.Z));
}

bool UForeverExplosionShakePattern::IsFinishedImpl() const
{
	return elapsed >= duration;
}

UForeverExplosionShake::UForeverExplosionShake(const FObjectInitializer& objectInitializer)
	: Super(objectInitializer)
{
	// RootShakePattern是private，只能经SetRootShakePattern设置。不能用ChangeRootShakePattern<T>()：
	// 它内部是匿名NewObject，在构造函数里会触发"empty name"致命错误，必须用CreateDefaultSubobject。
	SetRootShakePattern(CreateDefaultSubobject<UForeverExplosionShakePattern>(TEXT("Pattern")));
}
