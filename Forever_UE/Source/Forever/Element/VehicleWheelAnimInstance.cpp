#include "Element/VehicleWheelAnimInstance.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "GameFramework/Actor.h"
#include "Element/PlaneElement.h"

void FVehicleWheelAnimInstanceProxy::SetPropellerOwner(const APlaneElement* InPlane)
{
	PropellerOwner = InPlane;
}

void FVehicleWheelAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	if (const APlaneElement* Plane = PropellerOwner.Get()) {
		const FName PropellerBoneName = Plane->GetPropellerBoneName();
		if (PropellerBone.BoneName != PropellerBoneName) {
			PropellerBone.BoneName = PropellerBoneName;
			bBoneReferencesInitialized = false;
		}
		PropellerAngle = Plane->propellerAngle;
		PropellerAxis = Plane->GetPropellerAxis();
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
		PropellerBone.Initialize(BoneContainer);
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

	// 螺旋桨：同样后乘，绕骨骼自己的局部轴转(原因同上面车轮的说明)。
	if (PropellerOwner.IsValid() && PropellerBone.IsValidToEvaluate(BoneContainer)) {
		const FCompactPoseBoneIndex BoneIndex = PropellerBone.GetCompactPoseIndex(BoneContainer);
		FTransform& BoneTransform = Output.Pose[BoneIndex];
		const FQuat SpinQuat(PropellerAxis.GetSafeNormal(), FMath::DegreesToRadians(PropellerAngle));
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

	if (const APlaneElement* Plane = Cast<APlaneElement>(GetOwningActor())) {
		AnimInstanceProxy.SetPropellerOwner(Plane);
	}
}
