#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/BoneReference.h"
#include "ChaosWheeledVehicleMovementComponent.h"

#include "VehicleWheelAnimInstance.generated.h"

class APlaneElement;

// 每个轮子缓存的姿态数据——PreUpdate()(游戏线程)写，Evaluate()(可能在工作线程)读，不加锁(Proxy保证二者不同时跑)。
struct FVehicleWheelPose
{
	FBoneReference BoneReference;
	float DisplayRotationAngle = 0.f; // 实际拿去画的滚动角度(度，累加值)，限速见下
	float SteerAngle = 0.f;    // 前轮转向角度(度)，非前轮恒为0，来自GetSteerAngle()

	// 视觉限速：GetRotationAngle()是物理真实滚动角，高速时每帧变化量过大会转得像抽风，
	// 所以每帧只累加真实变化量和允许最大变化量里更小的那个(不影响驾驶物理)。
	float LastRawRotationAngle = 0.f;
	bool bHasLastRawAngle = false;
};

// 重写FAnimInstanceProxy::Evaluate()，绕开Animation Blueprint节点图，纯C++驱动轮子骨骼的滚动/转向(及飞机螺旋桨)，见VehicleWheelAnimInstance.md。
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

	// 飞机螺旋桨：owner是APlaneElement时设置，PreUpdate读它的旋转角/骨骼名/轴，Evaluate叠加到骨骼上。
	void SetPropellerOwner(const APlaneElement* InPlane); // 实现在cpp：header里APlaneElement只前向声明，TWeakObjectPtr赋值需要完整类型

private:
	TWeakObjectPtr<const APlaneElement> PropellerOwner;
	FBoneReference PropellerBone;
	float PropellerAngle = 0.f;
	FVector PropellerAxis = FVector(1.f, 0.f, 0.f);

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

	// 视觉滚动转速上限(度/秒)，默认1080(3圈/秒)是凭经验给的"肉眼还算连贯"的量级，不同车型可各调。
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
