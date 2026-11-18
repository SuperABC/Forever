#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForeverRocketProjectile.generated.h"

class UBoxComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UPointLightComponent;
class UProjectileMovementComponent;
class UAudioComponent;
class UForeverProjectileBridgeComponent;
class UMaterialInstanceDynamic;

// 尾焰留下的一团烟：球体网格+独占的动态材质，世界坐标(不跟着火箭弹走)，环形复用。
USTRUCT()
struct FRocketSmokePuff
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> mesh = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> material = nullptr;

	// 已存活秒数，<0=未启用(隐藏)。
	float age = -1.f;
};

// 火箭弹：由spawn_actor Change放出(位置=枪口，+X朝向=发射方向)，之后飞行/加速/尾焰/碰撞/爆炸全是自己的事。
// 撞到任何东西 -> 生成爆炸特效(AForeverExplosionEffect::Spawn) -> 通过UForeverProjectileBridgeComponent::ReportHit
// 把命中报给发射它的武器脚本(projectile_hit事件，脚本里用area_damage结算范围伤害) -> 自毁。
// 蓝图子类(如/Game/Asset/Projectile/BP_RPG7Rocket)只覆盖参数和网格，不需要连任何节点。详见ForeverRocketProjectile.md。
UCLASS()
class FOREVER_API AForeverRocketProjectile : public AActor
{
	GENERATED_BODY()

public:
	AForeverRocketProjectile();

	virtual void BeginPlay() override;
	virtual void Tick(float deltaSeconds) override;
	virtual void OnConstruction(const FTransform& transform) override;

	// 火箭弹网格，模型原点在尾端、弹头朝+X(尾焰从原点向-X喷)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rocket")
	TObjectPtr<UStaticMesh> rocketMesh;

	// 碰撞盒半尺寸(局部，cm)：Actor原点=盒子中心=火箭弹中心，X方向全长要和火箭弹网格长度差不多(网格尾端在-X半个盒长处)。
	UPROPERTY(EditAnywhere, Category = "Rocket")
	FVector boxHalfExtent = FVector(30.f, 8.f, 8.f);

	// 初速/最大速度(cm/s)与加速度(cm/s^2)，沿朝向直线飞行，无重力。
	UPROPERTY(EditAnywhere, Category = "Rocket")
	float initialSpeed = 1500.f;
	UPROPERTY(EditAnywhere, Category = "Rocket")
	float maxSpeed = 9000.f;
	UPROPERTY(EditAnywhere, Category = "Rocket")
	float acceleration = 6000.f;

	// 没撞到任何东西时的最长飞行秒数，到时静默销毁(不爆炸)。
	UPROPERTY(EditAnywhere, Category = "Rocket")
	float lifeSeconds = 8.f;

	// 爆炸：整体缩放(AForeverExplosionEffect::Spawn的scale，1=枪击大小)，特效蓝图路径留空=默认爆炸类。
	UPROPERTY(EditAnywhere, Category = "Explosion")
	float explosionScale = 3.f;
	UPROPERTY(EditAnywhere, Category = "Explosion")
	FString explosionEffectClassPath;

	// 报给脚本的爆心沿撞击面法线外推多少(cm)：贴墙爆炸时area_damage的遮挡射线才不会一出门就被这面墙挡住。特效仍放在原始撞击点。
	UPROPERTY(EditAnywhere, Category = "Explosion")
	float reportOffset = 10.f;

	// 尾焰(火焰球沿-X拉长，长度/半径cm，闪烁)与烟(沿途每隔smokeInterval秒留一团，半径从smokeRadiusStart胀到smokeRadiusEnd并淡出)。
	UPROPERTY(EditAnywhere, Category = "Flame")
	float flameLength = 90.f;
	UPROPERTY(EditAnywhere, Category = "Flame")
	float flameRadius = 7.f;
	// 火焰自发光显示亮度倍率，同AForeverExplosionEffect::emissiveScale(材质已抵消曝光)。
	UPROPERTY(EditAnywhere, Category = "Flame")
	float emissiveScale = 0.03f;
	UPROPERTY(EditAnywhere, Category = "Flame")
	float flameLightIntensity = 40000.f;
	UPROPERTY(EditAnywhere, Category = "Flame")
	float smokeInterval = 0.025f;
	UPROPERTY(EditAnywhere, Category = "Flame")
	float smokeLife = 1.2f;
	UPROPERTY(EditAnywhere, Category = "Flame")
	float smokeRadiusStart = 6.f;
	UPROPERTY(EditAnywhere, Category = "Flame")
	float smokeRadiusEnd = 40.f;
	UPROPERTY(EditAnywhere, Category = "Flame")
	float smokeBrightness = 0.25f;

	// 飞行持续音效：UE音效资产路径(不走脚本系统)，由自己的音频组件在Tick里连续播放，音调随速度升高；留空=无声。
	UPROPERTY(EditAnywhere, Category = "Sound")
	FString flightSoundPath = TEXT("/Game/Asset/Sound/rocket_flight");
	// 音量倍率，以及初速/最大速度时的音调倍率(中间按速度线性插值)。
	UPROPERTY(EditAnywhere, Category = "Sound")
	float flightVolume = 1.f;
	UPROPERTY(EditAnywhere, Category = "Sound")
	float flightPitchAtStart = 0.85f;
	UPROPERTY(EditAnywhere, Category = "Sound")
	float flightPitchAtMax = 1.25f;

private:
	UFUNCTION()
	void OnProjectileStop(const FHitResult& hit);

	// 火箭弹尾端(网格原点、尾焰起点)在Actor局部坐标的位置。
	FVector TailLocation() const { return FVector(-boxHalfExtent.X, 0.f, 0.f); }

	// 爆炸+报告命中+自毁，只执行一次。
	void Explode(const FHitResult& hit);
	void UpdateFlame(float deltaSeconds);
	void UpdateSmoke(float deltaSeconds);
	void UpdateFlightSound();

	static constexpr int32 SmokeCount = 48;

	UPROPERTY()
	TObjectPtr<UBoxComponent> box;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> meshComponent;
	UPROPERTY()
	TObjectPtr<UProjectileMovementComponent> movement;
	UPROPERTY()
	TObjectPtr<UForeverProjectileBridgeComponent> bridge;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> flame;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> flameMaterial;
	UPROPERTY()
	TObjectPtr<UPointLightComponent> flameLight;
	UPROPERTY()
	TObjectPtr<UAudioComponent> flightAudio;
	UPROPERTY()
	TArray<FRocketSmokePuff> smokes;

	bool bExploded = false;
	float age = 0.f;
	float smokeTimer = 0.f;
	int32 nextSmoke = 0;
};
