#include "Element/VehicleWheelAnimInstance.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "GameFramework/Actor.h"
#include "Element/VehicleElement.h"

void FVehicleWheelAnimInstanceProxy::SetSpinOwner(const AVehicleElement* InVehicle)
{
	SpinOwner = InVehicle;
}

void FVehicleWheelAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	bHasSpinBone = false;
	if (const AVehicleElement* Vehicle = SpinOwner.Get()) {
		FName SpinBoneName;
		if (Vehicle->GetSpinningBone(SpinBoneName, SpinAxis, SpinAngle)) {
			if (SpinBone.BoneName != SpinBoneName) {
				SpinBone.BoneName = SpinBoneName;
				bBoneReferencesInitialized = false;
			}
			bHasSpinBone = true;
		}
	}

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
	// FAnimInstanceProxy::Evaluate()是引擎留给"完全代码驱动、不需要蓝图节点图"场景的正式扩展点。
	Output.ResetToRefPose();

	const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();
	if (!bBoneReferencesInitialized) {
		for (FVehicleWheelPose& Pose : WheelPoses) {
			Pose.BoneReference.Initialize(BoneContainer);
		}
		SpinBone.Initialize(BoneContainer);
		bBoneReferencesInitialized = true;
	}

	for (const FVehicleWheelPose& Pose : WheelPoses) {
		if (!Pose.BoneReference.IsValidToEvaluate(BoneContainer)) continue;

		const FCompactPoseBoneIndex BoneIndex = Pose.BoneReference.GetCompactPoseIndex(BoneContainer);
		FTransform& BoneTransform = Output.Pose[BoneIndex];

		// 必须后乘：滚动/转向是绕轮子自己的轴转，要在骨骼自己的局部坐标系里叠加；预乘会让轮子转得七扭八歪。
		const FQuat RollSteerQuat(FRotator(Pose.DisplayRotationAngle, Pose.SteerAngle, 0.f));
		BoneTransform.SetRotation(BoneTransform.GetRotation() * RollSteerQuat);
	}

	// 一直转的骨骼(螺旋桨之类)：同样后乘，绕骨骼自己的局部轴转(原因同上面车轮的说明)。
	if (bHasSpinBone && SpinBone.IsValidToEvaluate(BoneContainer)) {
		const FCompactPoseBoneIndex BoneIndex = SpinBone.GetCompactPoseIndex(BoneContainer);
		FTransform& BoneTransform = Output.Pose[BoneIndex];
		const FQuat SpinQuat(SpinAxis.GetSafeNormal(), FMath::DegreesToRadians(SpinAngle));
		BoneTransform.SetRotation(BoneTransform.GetRotation() * SpinQuat);
	}

	return true;
}

void UVehicleWheelAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// owner是AVehicleElement而不是AWheeledVehiclePawn，所以按类型在owner身上找UChaosWheeledVehicleMovementComponent。
	if (AActor* Owner = GetOwningActor()) {
		if (UChaosWheeledVehicleMovementComponent* Component = Owner->FindComponentByClass<UChaosWheeledVehicleMovementComponent>()) {
			SetWheeledVehicleComponent(Component);
		}
	}
	AnimInstanceProxy.SetMaxWheelSpinDegreesPerSecond(MaxWheelSpinDegreesPerSecond);

	// owner是载具就让Proxy每帧问它有没有"一直转的骨骼"；不是载具(比如线路上的运动学车)就没有。
	AnimInstanceProxy.SetSpinOwner(Cast<AVehicleElement>(GetOwningActor()));
}
