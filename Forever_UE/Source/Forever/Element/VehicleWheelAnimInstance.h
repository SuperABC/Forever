#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/BoneReference.h"
#include "ChaosWheeledVehicleMovementComponent.h"

#include "VehicleWheelAnimInstance.generated.h"

// 每个轮子缓存的姿态数据——PreUpdate()(游戏线程)写，Evaluate()(可能在工作线程)读，
// 之间不做任何跨线程加锁，因为FAnimInstanceProxy本身保证PreUpdate和Evaluate不会同时跑。
struct FVehicleWheelPose
{
	FBoneReference BoneReference;
	float DisplayRotationAngle = 0.f; // 实际拿去画的滚动角度(度，累加值)——见下面"限速"说明
	float SteerAngle = 0.f;    // 前轮转向角度(度)，非前轮恒为0，来自GetSteerAngle()

	// UChaosVehicleWheel::GetRotationAngle()返回的是物理仿真里真实的车轮滚动角度，车速
	// 一高这个角度每帧变化量就很大，视觉上轮子转得像抽风(实测反馈"转速太高了渲染效果太
	// 差")——不是引擎在骗你，是真实物理转速本来就可以远超人眼觉得自然的范围。这里改成
	// 每帧只累加"物理真实变化量"和"允许的最大变化量"里更小的那个，相当于给视觉转速单独
	// 设一个上限，不影响实际驾驶手感(真实驾驶物理完全不受这个限制)。
	float LastRawRotationAngle = 0.f;
	bool bHasLastRawAngle = false;
};

// 重写FAnimInstanceProxy::Evaluate()，绕开Animation Blueprint节点图，纯C++驱动四个
// 轮子骨骼的滚动/转向——见VehicleElement.md"车轮滚动/转向动画"一节。
USTRUCT()
struct FOREVER_API FVehicleWheelAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FVehicleWheelAnimInstanceProxy() : FAnimInstanceProxy() {}
	FVehicleWheelAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	void SetWheeledVehicleComponent(const UChaosWheeledVehicleMovementComponent* InComponent) {
		WheeledVehicleComponent = InComponent;
	}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	TWeakObjectPtr<const UChaosWheeledVehicleMovementComponent> WheeledVehicleComponent;
	TArray<FVehicleWheelPose> WheelPoses;
	bool bBoneReferencesInitialized = false;
	float MaxWheelSpinDegreesPerSecond = 1080.f; // 视觉滚动转速上限(度/秒)，见FVehicleWheelPose注释

public:
	void SetMaxWheelSpinDegreesPerSecond(float InMax) { MaxWheelSpinDegreesPerSecond = InMax; }
};

UCLASS(transient)
class FOREVER_API UVehicleWheelAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	void SetWheeledVehicleComponent(const UChaosWheeledVehicleMovementComponent* InComponent) {
		WheeledVehicleComponent = InComponent;
		AnimInstanceProxy.SetWheeledVehicleComponent(InComponent);
	}

	// 视觉滚动转速上限，纯为了不同车型可以按轮子大小/预期观感各自调，默认1080度/秒(3圈/秒)
	// 是凭经验给的一个"肉眼看起来还算连贯"的量级。
	UPROPERTY(EditDefaultsOnly, Category = "Vehicle")
	float MaxWheelSpinDegreesPerSecond = 1080.f;

private:
	virtual void NativeInitializeAnimation() override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return &AnimInstanceProxy; }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override {}

	FVehicleWheelAnimInstanceProxy AnimInstanceProxy;

	UPROPERTY(transient)
	TObjectPtr<const UChaosWheeledVehicleMovementComponent> WheeledVehicleComponent;
};
