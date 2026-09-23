#include "Element/VehicleWheelAnimInstance.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "GameFramework/Actor.h"

void FVehicleWheelAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const UChaosWheeledVehicleMovementComponent* Component = WheeledVehicleComponent.Get();
	if (!Component) return;

	const int32 NumWheels = Component->WheelSetups.Num();
	if (WheelPoses.Num() != NumWheels) {
		WheelPoses.SetNum(NumWheels);
		bBoneReferencesInitialized = false; // 数量变了，骨骼引用要重新按新的BoneName Initialize
	}

	for (int32 i = 0; i < NumWheels; i++) {
		FVehicleWheelPose& Pose = WheelPoses[i];
		const FName BoneName = Component->WheelSetups[i].BoneName;
		if (Pose.BoneReference.BoneName != BoneName) {
			Pose.BoneReference.BoneName = BoneName;
			bBoneReferencesInitialized = false;
		}
		if (Component->Wheels.IsValidIndex(i) && Component->Wheels[i]) {
			const float RawAngle = Component->Wheels[i]->GetRotationAngle();
			if (Pose.bHasLastRawAngle) {
				const float RawDelta = FMath::FindDeltaAngleDegrees(Pose.LastRawRotationAngle, RawAngle);
				const float MaxDeltaThisTick = MaxWheelSpinDegreesPerSecond * DeltaSeconds;
				Pose.DisplayRotationAngle += FMath::Clamp(RawDelta, -MaxDeltaThisTick, MaxDeltaThisTick);
			} else {
				Pose.DisplayRotationAngle = RawAngle;
				Pose.bHasLastRawAngle = true;
			}
			Pose.LastRawRotationAngle = RawAngle;
			Pose.SteerAngle = Component->Wheels[i]->GetSteerAngle();
		}
	}
}

bool FVehicleWheelAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	// 重写引擎自带FAnimInstanceProxy::Evaluate()(AnimInstanceProxy.h默认实现{return
	// false;}，是官方留给"完全代码驱动、不需要蓝图节点图"场景的正式扩展点)。
	Output.ResetToRefPose();

	const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();
	if (!bBoneReferencesInitialized) {
		for (FVehicleWheelPose& Pose : WheelPoses) {
			Pose.BoneReference.Initialize(BoneContainer);
		}
		bBoneReferencesInitialized = true;
	}

	for (const FVehicleWheelPose& Pose : WheelPoses) {
		if (!Pose.BoneReference.IsValidToEvaluate(BoneContainer)) continue;

		const FCompactPoseBoneIndex BoneIndex = Pose.BoneReference.GetCompactPoseIndex(BoneContainer);
		FTransform& BoneTransform = Output.Pose[BoneIndex];

		// 踩过的坑：这里一开始写成"RollSteerQuat * BoneTransform.GetRotation()"(预乘)，
		// 结果轮子转起来七扭八歪——预乘是把RollSteerQuat当成"父骨骼坐标系下的旋转"叠加，
		// 而滚动/转向这个概念本来就是"绕轮子自己的轴转"，应该表达在轮子骨骼自己的本地
		// (bind pose)坐标系里，也就是要后乘：BoneTransform.GetRotation()先把"轮子自己
		// 的坐标系"变换到父骨骼坐标系，RollSteerQuat再在轮子自己的坐标系里转，顺序不能
		// 反。FAnimNode_WheelController.cpp效果上做的是同一件事，只是它工作在组件空间，
		// 需要先ConvertCSTransformToBoneSpace把变换重新表达到骨骼自己的坐标系里才能这样
		// 叠加，再ConvertBoneSpaceTransformToCS转回去；这里Evaluate()本来就是本地(父骨骼
		// 空间)的FPoseContext，不需要那一趟来回转换，但叠加顺序(后乘)是同一个道理，不能
		// 图省事写成预乘。
		const FQuat RollSteerQuat(FRotator(Pose.DisplayRotationAngle, Pose.SteerAngle, 0.f));
		BoneTransform.SetRotation(BoneTransform.GetRotation() * RollSteerQuat);
	}

	return true;
}

void UVehicleWheelAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 和引擎自带UVehicleAnimationInstance::NativeInitializeAnimation()一样的写法，只是
	// 这里的owner是AVehicleElement而不是AWheeledVehiclePawn，改成按类型在owner身上找
	// UChaosWheeledVehicleMovementComponent。
	if (AActor* Owner = GetOwningActor()) {
		if (UChaosWheeledVehicleMovementComponent* Component = Owner->FindComponentByClass<UChaosWheeledVehicleMovementComponent>()) {
			SetWheeledVehicleComponent(Component);
		}
	}
	AnimInstanceProxy.SetMaxWheelSpinDegreesPerSecond(MaxWheelSpinDegreesPerSecond);
}
