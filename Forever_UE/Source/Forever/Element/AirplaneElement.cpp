#include "Element/AirplaneElement.h"

#include "Element/AirplaneWheel.h"

#include "ChaosVehicleWheel.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/ForeverKeyBindingSubsystem.h"
#include "InputActionValue.h"

namespace {
	// AddOnScreenDebugMessage的key：同一个key新消息会替换旧消息，不会刷屏。9000/200/201/202已被
	// 游戏时钟/武器/暂停界面占用。
	constexpr int32 kHudKey = 210;
	constexpr float kCmPerSecToKmh = 0.036f; // cm/s -> m/s(0.01) -> km/h(3.6)
}

AAirplaneElement::AAirplaneElement()
{
	// 飞机是3个轮子(鼻轮+左右主轮)，不是基类默认的4个；骨骼名交给蓝图的WheelSetups填，顺序固定：
	// [0]鼻轮 [1]左主轮 [2]右主轮。
	vehicleMovement->WheelSetups.SetNum(3);
	vehicleMovement->WheelSetups[0].WheelClass = UAirplaneWheelNose::StaticClass();
	vehicleMovement->WheelSetups[1].WheelClass = UAirplaneWheelMain::StaticClass();
	vehicleMovement->WheelSetups[2].WheelClass = UAirplaneWheelMain::StaticClass();

	// 空气阻力/下压力由下面Tick里手写的飞行力学负责，关掉Chaos载具自带的一份，免得叠加。
	vehicleMovement->DragCoefficient = 0.f;
	vehicleMovement->DownforceCoefficient = 0.f;

	// 追尾视角：弹簧臂跟着机身一起转(翻滚/特技时画面跟着转)，鼠标只在这个基础上小幅环视。
	cameraBoom->bUsePawnControlRotation = false;
	cameraBoom->TargetArmLength = 1500.f;
	cameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	cameraBoom->SetRelativeRotation(FRotator(-10.f, 0.f, 0.f));
}

void AAirplaneElement::BeginPlay()
{
	Super::BeginPlay();

	// 机身不能是弹性碰撞：用恢复系数0、取两边较小值的物理材质覆盖，从高空摔下来不会弹起。
	if (carMesh) {
		UPhysicalMaterial* material = NewObject<UPhysicalMaterial>(this);
		material->Restitution = 0.f;
		material->bOverrideRestitutionCombineMode = true;
		material->RestitutionCombineMode = EFrictionCombineMode::Min;
		carMesh->SetPhysMaterialOverride(material);
	}

	// 重心偏移：物理状态此时已创建，直接改根骨刚体的COM nudge。
	if (carMesh && !centerOfMassOffset.IsNearlyZero()) {
		carMesh->SetCenterOfMass(centerOfMassOffset);
	}

	// 停机位里无人时用手刹+刹车拉住，不然停在斜坡上会自己滚走。
	if (vehicleMovement) {
		vehicleMovement->SetHandbrakeInput(true);
		vehicleMovement->SetBrakeInput(1.f);
	}
}

void AAirplaneElement::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	power = 0.f;
	pitchCmd = 0.f;
	rollCmd = 0.f;
	lookYaw = 0.f;
	lookPitch = 0.f;
	bFreeLook = false;
	lookIdleTime = 0.f;
	if (vehicleMovement) {
		vehicleMovement->SetHandbrakeInput(false);
		vehicleMovement->SetBrakeInput(0.f);
		vehicleMovement->SetThrottleInput(0.f); // 轮子不带动力，推力直接作用在机身上
	}
}

void AAirplaneElement::UnPossessed()
{
	// 基类会清零油门/转向并拉满刹车；这里再把飞行相关的持久输入清掉，不然没人开的飞机
	// 会带着最后一次的动力/舵量一直飞。
	power = 0.f;
	pitchCmd = 0.f;
	rollCmd = 0.f;
	Super::UnPossessed();
}

void AAirplaneElement::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// 故意绕过AVehicleElement::SetupPlayerInputComponent(它把W/S绑成油门/刹车)，直接用APawn的。
	APawn::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* enhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!enhancedInput) return;

	enhancedInput->BindAction(moveAction, ETriggerEvent::Triggered, this, &AAirplaneElement::MoveAirplane);
	enhancedInput->BindAction(moveAction, ETriggerEvent::Completed, this, &AAirplaneElement::StopMoveAirplane);
	enhancedInput->BindAction(moveAction, ETriggerEvent::Canceled, this, &AAirplaneElement::StopMoveAirplane);
	enhancedInput->BindAction(lookAction, ETriggerEvent::Triggered, this, &AAirplaneElement::LookAirplane);
	enhancedInput->BindAction(mouseLookAction, ETriggerEvent::Triggered, this, &AAirplaneElement::LookAirplane);

	if (UGameInstance* gameInstance = GetGameInstance()) {
		if (UForeverKeyBindingSubsystem* keyBindings = gameInstance->GetSubsystem<UForeverKeyBindingSubsystem>()) {
			enhancedInput->BindAction(keyBindings->GetAction(TEXT("ExitVehicle")), ETriggerEvent::Started, this, &AAirplaneElement::AirplaneExit);
			enhancedInput->BindAction(keyBindings->GetAction(TEXT("FreeLook")), ETriggerEvent::Started, this, &AAirplaneElement::StartFreeLook);
			enhancedInput->BindAction(keyBindings->GetAction(TEXT("FreeLook")), ETriggerEvent::Completed, this, &AAirplaneElement::StopFreeLook);
			enhancedInput->BindAction(keyBindings->GetAction(TEXT("FreeLook")), ETriggerEvent::Canceled, this, &AAirplaneElement::StopFreeLook);
			enhancedInput->BindAction(keyBindings->GetAction(TEXT("Handbrake")), ETriggerEvent::Started, this, &AAirplaneElement::AirplaneHandbrake);
			enhancedInput->BindAction(keyBindings->GetAction(TEXT("Handbrake")), ETriggerEvent::Completed, this, &AAirplaneElement::AirplaneStopHandbrake);
			enhancedInput->BindAction(keyBindings->GetAction(TEXT("Handbrake")), ETriggerEvent::Canceled, this, &AAirplaneElement::AirplaneStopHandbrake);
			enhancedInput->BindAction(keyBindings->GetAction(TEXT("ThrottleUp")), ETriggerEvent::Started, this, &AAirplaneElement::ThrottleUp);
			enhancedInput->BindAction(keyBindings->GetAction(TEXT("ThrottleDown")), ETriggerEvent::Started, this, &AAirplaneElement::ThrottleDown);
		}
	}
}

void AAirplaneElement::MoveAirplane(const FInputActionValue& value)
{
	const FVector2D movement = value.Get<FVector2D>();

	pitchCmd = -movement.Y; // W(Y>0)=机头向下，S(Y<0)=机头抬升
	rollCmd = movement.X;   // D(X>0)=向右滚转
	// A/D同时给轮子转向：在地面轮子起作用，在空中轮子悬空无效；滚转反过来只在空中起作用(见Tick)，互不影响。
	vehicleMovement->SetSteeringInput(movement.X);
}

void AAirplaneElement::StopMoveAirplane(const FInputActionValue& value)
{
	pitchCmd = 0.f;
	rollCmd = 0.f;
	vehicleMovement->SetSteeringInput(0.f);
}

void AAirplaneElement::LookAirplane(const FInputActionValue& value)
{
	const FVector2D look = value.Get<FVector2D>();
	lookYaw = FRotator::NormalizeAxis(lookYaw + look.X); // 一圈都能转，可以看正后方
	// 和AddControllerPitchInput同一个符号约定(PlayerController的InputPitchScale是负数)：Look输入的Y
	// 为正是"往下看"，而FRotator的俯仰为正是"往上看"，所以这里要减。
	lookPitch = FMath::Clamp(lookPitch - look.Y, -80.f, 80.f);
	bLookedThisFrame = true;
}

void AAirplaneElement::ThrottleUp()
{
	power = FMath::Clamp(power + powerStep, minPower, 100.f);
}

void AAirplaneElement::ThrottleDown()
{
	power = FMath::Clamp(power - powerStep, minPower, 100.f);
}

void AAirplaneElement::AirplaneExit()
{
	ExitVehicle();
}

void AAirplaneElement::AirplaneHandbrake(const FInputActionValue& value)
{
	Handbrake(value);
}

void AAirplaneElement::AirplaneStopHandbrake(const FInputActionValue& value)
{
	StopHandbrake(value);
}

bool AAirplaneElement::IsAirborne() const
{
	if (!vehicleMovement || vehicleMovement->Wheels.IsEmpty()) return true;
	for (const UChaosVehicleWheel* wheel : vehicleMovement->Wheels) {
		if (wheel && !wheel->IsInAir()) return false;
	}
	return true;
}

void AAirplaneElement::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime); // 把位置写回Core，见AVehicleElement::Tick

	if (!carMesh) return;

	// 环视偏移：鼠标不动时缓慢回正，追尾视角本身(俯仰-10度)由弹簧臂的相对旋转给出。
	if (bLookedThisFrame || bFreeLook) {
		lookIdleTime = 0.f;
	} else {
		lookIdleTime += DeltaTime;
		if (lookIdleTime >= recenterDelay) {
			lookYaw = FMath::FInterpTo(lookYaw, 0.f, DeltaTime, recenterSpeed);
			lookPitch = FMath::FInterpTo(lookPitch, 0.f, DeltaTime, recenterSpeed);
		}
	}
	bLookedThisFrame = false;
	cameraBoom->SetRelativeRotation(FRotator(-10.f + lookPitch, lookYaw, 0.f));

	// HUD放在物理判断前面：就算刚体没在模拟(蓝图/物理资产没配好)，也要能看到动力和速度，方便排查。
	UpdateHud(GetVelocity().Size());

	// IsSimulatingPhysics()对骨骼网格按根骨查body，物理资产配置不同时可能误判，用IsAnySimulatingPhysics()。
	if (!carMesh->IsAnySimulatingPhysics()) {
		if (IsPlayerControlled() && !bWarnedNotSimulating) {
			bWarnedNotSimulating = true;
			UE_LOG(LogTemp, Warning, TEXT("AAirplaneElement: carMesh没有在模拟物理，飞行力学不生效。检查物理资产的Root body是否Simulated。"));
		}
		return;
	}

	const FTransform transform = carMesh->GetComponentTransform();
	const FVector forward = transform.GetUnitAxis(EAxis::X);
	const FVector right = transform.GetUnitAxis(EAxis::Y);
	const FVector up = transform.GetUnitAxis(EAxis::Z);

	const FVector velocity = carMesh->GetPhysicsLinearVelocity();
	const float speed = velocity.Size();
	const float forwardSpeed = FVector::DotProduct(velocity, forward);

	// 无人且几乎停住时不施力，让刚体能睡眠(AddForce会唤醒刚体)。
	if (!IsPlayerControlled() && speed < 5.f && power == 0.f) return;

	const bool bAirborne = IsAirborne();
	const float gravity = FMath::Abs(GetWorld()->GetGravityZ());

	// ---- 线加速度(cm/s²) ----
	FVector accel = forward * (power * 0.01f * maxThrustAccel);

	// 阻力：k*|v|*v，k由满动力极速反推(满动力时推力=阻力)。
	const float dragK = maxThrustAccel / FMath::Max(topSpeed * topSpeed, 1.f);
	accel -= velocity * (speed * dragK);

	// 升力：沿机体"上"，正比于机头方向速度平方，stallSpeed时刚好等于重力；倒飞时朝下，需要推杆。
	if (forwardSpeed > 0.f) {
		const float ratio = forwardSpeed / FMath::Max(stallSpeed, 1.f);
		// 基础升力封顶在重力：速度再高也只是"刚好托住"，松杆平飞不会爬升；多余升力只来自迎角(机头高于气流)。
		const float baseLift = gravity * FMath::Min(ratio * ratio, 1.f);
		const float sinAoa = speed > 1.f ? -FVector::DotProduct(velocity, up) / speed : 0.f;
		const float aoaLift = gravity * aoaLiftGain * sinAoa * ratio * ratio;
		accel += up * FMath::Min(baseLift + aoaLift, maxLiftAccel);
	}

	// 速度方向向机头对齐(空中)：权限随空速增长，低速时不起作用，免得起飞前原地"飘"。
	const float airAuthority = FMath::Clamp(forwardSpeed / FMath::Max(stallSpeed, 1.f), 0.f, 1.f);
	if (bAirborne && forwardSpeed > 0.f) {
		accel += (forward * speed - velocity) * (alignRate * airAuthority);
	}
	carMesh->AddForce(accel, NAME_None, true);

	// ---- 角加速度(rad/s²，世界坐标) ----
	// 约定(UE坐标系下由 ω×r 推出)：绕-右轴转=机头抬升，绕-前轴转=右翼下沉(向右滚转)。
	const float controlAuthority = FMath::Clamp(forwardSpeed / FMath::Max(controlSpeed, 1.f), 0.f, 1.f);
	FVector angularAccel = -right * (pitchCmd * pitchAccel * controlAuthority);
	if (bAirborne) {
		// 地面上轮子着地无法滚转，只有离地后滚转才起作用。
		angularAccel += -forward * (rollCmd * rollAccel * controlAuthority);
	}

	// 风标：机头朝速度方向转，让倾斜转弯时机头跟着转向，侧滑时自动修正。
	if (bAirborne && speed > 100.f && forwardSpeed > 0.f) {
		angularAccel += FVector::CrossProduct(forward, velocity / speed) * (weathervaneAccel * airAuthority);
	}

	// 失速：空中前向速度不到stallSpeed时舵面和升力都撑不住，机头被重力压向地面，越慢压得越狠。
	// 轴=机头叉向下方向；机头垂直朝上时叉积为0，补一点沿右轴的低头偏置让它能翻下来。
	if (bAirborne) {
		const float stall = 1.f - airAuthority;
		if (stall > 0.f) {
			FVector axis = FVector::CrossProduct(forward, FVector(0.f, 0.f, -1.f));
			if (forward.Z > 0.f) axis += right * 0.25f * forward.Z;
			angularAccel += axis * (stallPitchAccel * stall);
		}
	}

	// 角速度阻尼(按机体局部三轴)，控制输入松开后旋转自己停下。
	const FVector angularVelocity = carMesh->GetPhysicsAngularVelocityInRadians();
	const FVector localAngular = transform.InverseTransformVectorNoScale(angularVelocity);
	const FVector localDamping(-localAngular.X * rollDamping, -localAngular.Y * pitchDamping, -localAngular.Z * yawDamping);
	angularAccel += transform.TransformVectorNoScale(localDamping);

	carMesh->AddTorqueInRadians(angularAccel, NAME_None, true);
}

void AAirplaneElement::UpdateHud(float speedCmPerSec)
{
	if (!GEngine || !IsPlayerControlled()) return;

	// 高度取离地高度：从机身向下打一根射线，打不到地(比如在海上/射线太短)就退回世界Z。
	const FVector location = GetActorLocation();
	float altitudeMeters = location.Z * 0.01f;
	FHitResult hit;
	FCollisionQueryParams params(SCENE_QUERY_STAT(AirplaneAltitude), false, this);
	if (GetWorld()->LineTraceSingleByChannel(hit, location, location - FVector(0.f, 0.f, 300000.f), ECC_WorldStatic, params)) {
		altitudeMeters = hit.Distance * 0.01f;
	}

	GEngine->AddOnScreenDebugMessage(kHudKey, 0.1f, FColor::White,
		FString::Printf(TEXT("动力 %.0f%%   速度 %.0f km/h   高度 %.0f m"), power, speedCmPerSec * kCmPerSecToKmh, altitudeMeters));
}
