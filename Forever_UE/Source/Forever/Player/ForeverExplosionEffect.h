#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ForeverExplosionEffect.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

// 一个爆炸特效部件：网格+它独占的动态材质(材质为空时只有网格，颜色不可控)。
USTRUCT()
struct FExplosionPart
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> mesh = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> material = nullptr;
};

// 命中点的程序化爆炸特效：火球+冲击波环+火花+闪光+烟，全部由引擎内置球体网格和
// PointLight在Tick里驱动，不依赖任何手工制作的.uasset(没有Niagara)。播放约totalDuration
// 秒后自动Destroy。只做视觉，不产生伤害/冲击力。详见ForeverExplosionEffect.md。
UCLASS()
class FOREVER_API AForeverExplosionEffect : public AActor
{
	GENERATED_BODY()

public:
	AForeverExplosionEffect();

	// 唯一入口：在location生成一次爆炸。normal是命中面法线(冲击波环贴着这个面)，
	// scale整体缩放所有尺寸/速度(1=默认大小)。
	static void Spawn(UWorld* world, const FVector& location, const FVector& normal, float scale = 1.f);

	virtual void BeginPlay() override;
	virtual void Tick(float deltaSeconds) override;

	// 以下均为未缩放(scale=1)时的值，单位cm/秒。
	UPROPERTY(EditAnywhere, Category = "Explosion")
	float totalDuration = 1.4f;

	UPROPERTY(EditAnywhere, Category = "Explosion")
	float fireballRadius = 60.f;
	UPROPERTY(EditAnywhere, Category = "Explosion")
	float fireballDuration = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Explosion")
	float ringRadius = 220.f;
	UPROPERTY(EditAnywhere, Category = "Explosion")
	float ringDuration = 0.35f;

	// 火球/环/火花自发光的统一倍率。材质里已乘Eye Adaptation Inverse抵消自动曝光，所以这里
	// 的数值就是"画面上直接显示的亮度"，昼夜通用。
	UPROPERTY(EditAnywhere, Category = "Explosion")
	float emissiveScale = 0.03f;

	// 烟的显示亮度倍率(烟材质的Color直接就是自发光，同样已抵消曝光)。
	UPROPERTY(EditAnywhere, Category = "Explosion")
	float smokeBrightness = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Explosion")
	float flashIntensity = 150000.f;
	UPROPERTY(EditAnywhere, Category = "Explosion")
	float flashDuration = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Explosion")
	float sparkSpeedMin = 300.f;
	UPROPERTY(EditAnywhere, Category = "Explosion")
	float sparkSpeedMax = 900.f;
	UPROPERTY(EditAnywhere, Category = "Explosion")
	float sparkLifeMin = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Explosion")
	float sparkLifeMax = 0.9f;

	UPROPERTY(EditAnywhere, Category = "Explosion")
	float smokeRadius = 35.f;
	// 单个烟球的最大不透明度。烟球会互相叠加，数值大了重叠处会发实。
	UPROPERTY(EditAnywhere, Category = "Explosion")
	float smokeOpacity = 0.15f;
	UPROPERTY(EditAnywhere, Category = "Explosion")
	float smokeRiseSpeed = 70.f;

private:
	static constexpr int32 SparkCount = 12;
	static constexpr int32 SmokeCount = 10;

	// isSmoke决定用烟材质(半透明)还是火焰材质(Additive自发光)。
	FExplosionPart MakePart(const TCHAR* name, bool isSmoke);
	void SetPartColor(FExplosionPart& part, const FLinearColor& color) const;
	void SetPartOpacity(FExplosionPart& part, float opacity) const;
	// 仅火焰材质有这个参数(自发光亮度倍数，>1时配合Bloom产生辉光)。
	void SetPartEmissive(FExplosionPart& part, float strength) const;
	// 球体网格直径100cm，按目标半径设世界缩放。
	void SetPartRadius(FExplosionPart& part, float radius) const;

	void UpdateFireball();
	void UpdateRing();
	void UpdateFlash();
	void UpdateSparks(float deltaSeconds);
	void UpdateSmoke();

	UPROPERTY()
	TObjectPtr<USceneComponent> root;

	UPROPERTY()
	FExplosionPart fireball;
	UPROPERTY()
	FExplosionPart ring;
	UPROPERTY()
	TArray<FExplosionPart> sparks;
	UPROPERTY()
	TArray<FExplosionPart> smokes;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> flash;

	FVector impactNormal = FVector::UpVector;
	float effectScale = 1.f;
	float age = 0.f;

	// 火花/烟的状态，坐标相对Actor原点(Actor不旋转，等价世界偏移)。
	TArray<FVector> sparkPosition;
	TArray<FVector> sparkVelocity;
	TArray<float> sparkLife;
	TArray<FVector> smokeOffset;
	TArray<float> smokeDelay;
};
