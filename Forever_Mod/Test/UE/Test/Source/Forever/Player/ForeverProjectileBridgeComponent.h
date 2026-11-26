#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ForeverProjectileBridgeComponent.generated.h"

// 命中报告的处理者(参数：所有者脚本键、命中点、命中面法线、被击中的市民姓名)。主程序在故事组件里绑定它，
// 把命中变成projectile_hit事件发给所有者脚本；迷你工程的Forever桩模块不绑定，所以组件在编辑器里只是个能选的类。
DECLARE_DELEGATE_FourParams(FForeverProjectileHitHandler, const FString& /*OwnerScriptKey*/, const FVector& /*Hit*/,
	const FVector& /*Normal*/, const FString& /*Sufferer*/);

// 弹射物(火箭弹等mod蓝图Actor)和脚本之间的桥。蓝图自己用引擎自带的ProjectileMovementComponent/BoxComponent实现
// 飞行、碰撞、尾焰、爆炸特效，撞到东西时调ReportHit，由所有者(发射它的武器)的脚本在projectile_hit事件里用
// spawn_effect/area_damage这些Change完成特效和伤害——蓝图不需要知道Change的存在，也不能直接调它。
// 只依赖UE类型(不依赖Core)，Forever_Mod/Test迷你工程的Forever桩模块里硬链接了同一份文件，见ForeverProjectileBridgeComponent.md。
UCLASS(ClassGroup = (Forever), meta = (BlueprintSpawnableComponent))
class FOREVER_API UForeverProjectileBridgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// 所有者脚本的稳定键(如"weapon:<武器id>")，由放置弹射物的Change(以后的spawn_actor)在生成后赋值。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forever")
	FString OwnerScriptKey;

	// 报告一次命中：Hit=命中点，Normal=命中面法线，Sufferer=被击中的市民姓名(没有则空)。
	UFUNCTION(BlueprintCallable, Category = "Forever")
	void ReportHit(FVector Hit, FVector Normal, FString Sufferer);

	// 主程序绑定的处理者(全局唯一)。
	static FForeverProjectileHitHandler& HitHandler();
};
