#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include <string>

#include "ForeverBulletSubsystem.generated.h"

class UInstancedStaticMeshComponent;

// 一颗飞行中的子弹：纯数据，不是Actor。命中、飞出射程都在UForeverBulletSubsystem::Tick里结算。
struct FBullet
{
	// 伤害随飞行距离的采样点数：发射时把WeaponMod::ComputeDamage在[0,maxRange]上等距采样存进来，
	// 命中时线性插值。这样mod override的衰减曲线也生效，且子弹不依赖Weapon对象的生命周期(换枪会销毁它)。
	static constexpr int32 DamageSamples = 9;

	FVector position = FVector::ZeroVector;
	FVector velocity = FVector::ZeroVector;
	float traveled = 0.f;
	float lastMove = 0.f; // 最近一次Tick飞过的距离，曳光弹尾巴至少拉到这么长
	float maxRange = 0.f;
	float gravityScale = 1.f;

	TWeakObjectPtr<AActor> shooter;
	TArray<TWeakObjectPtr<AActor>> ignored; // 扫掠时忽略：射手自己和挂在他身上的Actor(武器模型)
	std::string weaponType;
	float damageCurve[DamageSamples] = {};

	float SampleDamage(float distance) const;
};

// 子弹仿真器：逐帧(固定子步)积分重力、对每个子步的位移做线段扫掠，打中市民才结算伤害，
// 打中别的东西(墙/地面)或飞出射程就消失。弹道的视觉(曳光弹)也在这里，见ForeverBulletSubsystem.md。
UCLASS()
class FOREVER_API UForeverBulletSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void AddBullet(FBullet&& bullet);

	// 命中结算：命中的是ACitizenElement才有效果(打墙/落空什么都不发生)——伤害、爆炸特效，致死则广播
	// CitizenDeceaseEvent并处理死亡。Hitscan和子弹命中共用这一份。shooter可以已经失效(射手死了/销毁了)。
	static void ApplyCitizenHit(UWorld* world, AActor* shooter, const std::string& weaponType, float damage,
		const FHitResult& hit);

private:
	void EnsureTracer();
	void UpdateTracer();

	TArray<FBullet> bullets;

	// 曳光弹：一个ISM承载所有子弹的实例，每帧重建实例变换。
	UPROPERTY(Transient)
	TObjectPtr<AActor> tracerHost;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> tracerMesh;

	bool tracerHasInstances = false;
};
