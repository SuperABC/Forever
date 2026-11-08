#include "Element/VehicleElement.h"

#include "traffic/vehicle.h"
#include "traffic/route.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"
#include "Player/ForeverPlayerController.h"
#include "Player/ForeverCharacter.h"
#include "UI/MeetOptionWidget.h"

#include "Element/VehicleWheelFront.h"
#include "Element/VehicleWheelRear.h"
#include "Element/VehicleWheelAnimInstance.h"

#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/ForeverKeyBindingSubsystem.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AVehicleElement::AVehicleElement()
{
	PrimaryActorTick.bCanEverTick = true;

	carMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CarMesh"));
	RootComponent = carMesh;
	carMesh->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
	// 直接写BodyInstance.bSimulatePhysics字段，不调用SetSimulatePhysics()：后者会查物理材质，
	// 而构造函数在CDO构造阶段(GEngine未初始化)也会跑，会报"GEngine not initialized"。
	// 必须是true(与AWheeledVehiclePawn原版不同)：底盘骨骼在物理资产里不一定是Simulated，
	// 留false车能创建但不响应油门、不受重力(Chaos发现刚体不是Dynamic就跳过整个模拟)，见VehicleElement.md。
	carMesh->BodyInstance.bSimulatePhysics = true;
	carMesh->SetGenerateOverlapEvents(true);
	carMesh->SetCanEverAffectNavigation(false);

	// 纯C++驱动轮子滚动/转向，不需要Animation Blueprint，见VehicleWheelAnimInstance.md。
	carMesh->SetAnimInstanceClass(UVehicleWheelAnimInstance::StaticClass());

	vehicleMovement = CreateDefaultSubobject<UChaosWheeledVehicleMovementComponent>(TEXT("VehicleMovement"));
	vehicleMovement->SetUpdatedComponent(carMesh);
	vehicleMovement->WheelSetups.SetNum(4);
	vehicleMovement->WheelSetups[0].WheelClass = UVehicleWheelFront::StaticClass();
	vehicleMovement->WheelSetups[1].WheelClass = UVehicleWheelFront::StaticClass();
	vehicleMovement->WheelSetups[2].WheelClass = UVehicleWheelRear::StaticClass();
	vehicleMovement->WheelSetups[3].WheelClass = UVehicleWheelRear::StaticClass();

	// 骨骼网格和WheelSetups[i].BoneName故意不在这里赋值，由蓝图子类的CDO提供(时序原因见VehicleElement.md)。

	// 默认扭矩曲线是空的，FillEngineSetup()按它的MaxVal归一化会除0出NaN，所以手写一条普通形状的曲线，
	// 数值只是能跑起来的合理默认(加速手感不满意先调这里)。
	FRichCurve* torqueCurve = vehicleMovement->EngineSetup.TorqueCurve.GetRichCurve();
	torqueCurve->AddKey(0.f, 400.f);
	torqueCurve->AddKey(1890.f, 500.f);
	torqueCurve->AddKey(5730.f, 400.f);
	vehicleMovement->EngineSetup.MaxTorque = 500.f;
	vehicleMovement->EngineSetup.MaxRPM = 5730.f;

	cameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	cameraBoom->SetupAttachment(RootComponent);
	// 骨骼网格原点通常在车轮接地点，不抬高的话镜头会缩进底盘；但偏移跟着车身转，翻车时过大会让相机钻进地里，
	// 所以默认只抬50cm，个别车型可在蓝图子类里覆盖。
	cameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	cameraBoom->TargetArmLength = 600.0f;
	cameraBoom->bUsePawnControlRotation = true;

	followCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	followCamera->SetupAttachment(cameraBoom, USpringArmComponent::SocketName);
	followCamera->bUsePawnControlRotation = false;

	// 和AForeverCharacter指向同一份输入资源。
	static ConstructorHelpers::FObjectFinder<UInputAction> moveFinder(
		TEXT("/Game/Blueprint/Player/Input/Actions/IA_Move.IA_Move"));
	if (moveFinder.Succeeded()) {
		moveAction = moveFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> lookFinder(
		TEXT("/Game/Blueprint/Player/Input/Actions/IA_Look.IA_Look"));
	if (lookFinder.Succeeded()) {
		lookAction = lookFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> mouseLookFinder(
		TEXT("/Game/Blueprint/Player/Input/Actions/IA_MouseLook.IA_MouseLook"));
	if (mouseLookFinder.Succeeded()) {
		mouseLookAction = mouseLookFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> inputMappingFinder(
		TEXT("/Game/Blueprint/Player/Input/IMC_Default.IMC_Default"));
	if (inputMappingFinder.Succeeded()) {
		inputMapping = inputMappingFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> inputLookMappingFinder(
		TEXT("/Game/Blueprint/Player/Input/IMC_MouseLook.IMC_MouseLook"));
	if (inputLookMappingFinder.Succeeded()) {
		inputLookMapping = inputLookMappingFinder.Object;
	}
}

void AVehicleElement::Init(Vehicle* inVehicle, APawn* inPreviousPawn)
{
	// 外观由蓝图子类决定，这里只记住上下车要用到的两个引用。
	vehicle = inVehicle;
	previousPawn = inPreviousPawn;
	BuildProximityBox();

	// 预置车辆没人操控时刚体处于睡眠、不受重力，会悬在半空直到第一次踩油门才落地，所以生成后主动唤醒一次。
	if (carMesh) carMesh->WakeAllRigidBodies();
}

void AVehicleElement::BeginPlay()
{
	Super::BeginPlay();

	// 重心偏移和轮子半径是蓝图按车型填的参数，放BeginPlay而不是Init(Init只有交通系统生成才调，
	// 直接摆在关卡里的车走不到)。重心：直接改根骨刚体的COM nudge(同APlaneElement::BeginPlay)。
	if (carMesh && !centerOfMassOffset.IsNearlyZero()) {
		carMesh->SetCenterOfMass(centerOfMassOffset);
	}

	// 轮子半径在轮子类的CDO里，Chaos建物理时读CDO，没法按实例改；这里在物理建好之后按轮位覆盖。
	if (vehicleMovement && wheelRadius > 0.f) {
		for (int32 i = 0; i < vehicleMovement->WheelSetups.Num(); i++) vehicleMovement->SetWheelRadius(i, wheelRadius);
	}
}

void AVehicleElement::BuildProximityBox()
{
	if (!vehicle || proximityBox) return;
	// 没有脚本、也没有任何选项就不挂弹选项的触发盒；之后选项出现时RefreshOptions会补建。
	if (!vehicle->GetScript() && vehicle->GetOptions().empty()) return;

	UBoxComponent* box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
	FVector boxExtent(250.f, 250.f, 120.f);
	FVector boxCenter = FVector::ZeroVector;
	if (vehicle->GetCategory() != "car" && carMesh) {
		// 公交这类比轿车大得多的车型：触发盒取车身自己的局部包围盒再外扩一圈，刚好比模型大一点。
		const FBoxSphereBounds localBounds = carMesh->CalcBounds(FTransform::Identity);
		boxExtent = localBounds.BoxExtent + FVector(150.f, 150.f, 50.f);
		boxCenter = localBounds.Origin;
	}
	box->SetBoxExtent(boxExtent);
	box->SetRelativeLocation(boxCenter);
	box->SetCollisionProfileName(TEXT("Trigger"));
	box->SetupAttachment(RootComponent);
	box->OnComponentBeginOverlap.AddDynamic(this, &AVehicleElement::OnOverlapBegin);
	box->OnComponentEndOverlap.AddDynamic(this, &AVehicleElement::OnOverlapEnd);
	box->RegisterComponent();

	// 只烘焙一份快照，Overlap回调绝不解引用vehicle(同ACitizenElement)；此时game_start广播已跑完，选项已稳定。
	vehicleNameOnly = UTF8_TO_TCHAR(vehicle->GetName().c_str());
	cachedOptions.Reset();
	for (const std::string& option : vehicle->GetOptions()) {
		cachedOptions.Add(UTF8_TO_TCHAR(option.c_str()));
	}

	proximityBox = box;
}

void AVehicleElement::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	// 必须要求OtherActor是AForeverCharacter(人物)：否则开着车靠近另一辆停着的车也会弹"上车"选项。
	if (!pawn || OtherActor != pawn || OtherActor == this || !Cast<AForeverCharacter>(OtherActor)) return;

	if (AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))) {
		if (UMeetOptionWidget* meetOption = playerController->GetMeetOptionWidget()) {
			AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(
				UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
			meetOption->RemoveName(vehicleNameOnly); // 先清再加，重复的Begin事件不会叠出两份
			for (int32 idx = 0; idx < cachedOptions.Num(); idx++) {
				meetOption->AddOption(frameworkActor, cachedOptions[idx], vehicleNameOnly, idx);
			}
		}
	}
}

void AVehicleElement::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	// 和OnOverlapBegin同一条过滤，见那边的注释。
	if (!pawn || OtherActor != pawn || OtherActor == this || !Cast<AForeverCharacter>(OtherActor)) return;

	if (AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))) {
		if (UMeetOptionWidget* meetOption = playerController->GetMeetOptionWidget()) {
			meetOption->RemoveName(vehicleNameOnly);
		}
	}
}

void AVehicleElement::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 被销毁（流式离开/切回运动学）时把屏幕上这辆车名下的选项撤掉——组件销毁不一定补发End重叠事件。
	if (EndPlayReason == EEndPlayReason::Destroyed && !vehicleNameOnly.IsEmpty() && !bRideMode) HideOptions();
	vehicle = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AVehicleElement::SetDriveInput(float throttle, float brake, float steer)
{
	if (!vehicleMovement) return;
	vehicleMovement->SetThrottleInput(throttle);
	vehicleMovement->SetBrakeInput(brake);
	vehicleMovement->SetSteeringInput(steer);
}

void AVehicleElement::SetHandbrake(bool bOn)
{
	if (vehicleMovement) vehicleMovement->SetHandbrakeInput(bOn);
}

void AVehicleElement::DisableWheelBodies()
{
	// SetSimulatePhysics(true)会把Chaos初始化时(FixupSkeletalMesh)特意关掉的轮子刚体也重新打开，
	// 它们会变成独立刚体留在原地；这里再关回去，由Chaos悬挂/动画驱动。
	if (!carMesh || !vehicleMovement) return;
	for (const FChaosWheelSetup& setup : vehicleMovement->WheelSetups) {
		if (setup.BoneName == NAME_None) continue;
		if (FBodyInstance* wheelBody = carMesh->GetBodyInstance(setup.BoneName)) {
			wheelBody->SetInstanceSimulatePhysics(false);
		}
	}
}

void AVehicleElement::SetKinematicMode(bool bKinematic)
{
	if (bKinematicMode == bKinematic || !carMesh) return;
	bKinematicMode = bKinematic;
	if (bKinematic) {
		SetDriveInput(0.f, 0.f, 0.f);
		SetHandbrake(false);
		carMesh->SetSimulatePhysics(false);
	}
}

void AVehicleElement::ResumePhysics(const FVector& velocity)
{
	if (!carMesh) return;
	bKinematicMode = false;
	carMesh->SetSimulatePhysics(true);
	DisableWheelBodies();
	carMesh->SetPhysicsLinearVelocity(velocity);
	carMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	carMesh->WakeAllRigidBodies();
}

float AVehicleElement::GetForwardSpeed() const
{
	if (!carMesh || bKinematicMode) return 0.f;
	return FVector::DotProduct(carMesh->GetPhysicsLinearVelocity(), GetActorForwardVector());
}

float AVehicleElement::ComputeWheelbase() const
{
	if (!carMesh || !vehicleMovement || vehicleMovement->WheelSetups.Num() < 4) return 0.f;
	// 约定：WheelSetups[0..1]前轮，[2..3]后轮(见构造函数)。
	auto boneLocal = [this](int32 index) {
		return GetActorTransform().InverseTransformPosition(carMesh->GetBoneLocation(vehicleMovement->WheelSetups[index].BoneName));
	};
	const FVector front = (boneLocal(0) + boneLocal(1)) * 0.5f;
	const FVector rear = (boneLocal(2) + boneLocal(3)) * 0.5f;
	return FMath::Abs(front.X - rear.X);
}

FVector AVehicleElement::GetFrontAxleLocation() const
{
	if (carMesh && vehicleMovement && vehicleMovement->WheelSetups.Num() >= 2 &&
		vehicleMovement->WheelSetups[0].BoneName != NAME_None && vehicleMovement->WheelSetups[1].BoneName != NAME_None) {
		return (carMesh->GetBoneLocation(vehicleMovement->WheelSetups[0].BoneName) +
			carMesh->GetBoneLocation(vehicleMovement->WheelSetups[1].BoneName)) * 0.5f;
	}
	return GetActorLocation() + GetActorForwardVector() * (ComputeWheelbase() * 0.5f);
}

float AVehicleElement::GetMaxSteerAngleDegrees() const
{
	if (!vehicleMovement || vehicleMovement->WheelSetups.Num() == 0) return 0.f;
	TSubclassOf<UChaosVehicleWheel> wheelClass = vehicleMovement->WheelSetups[0].WheelClass;
	if (const UChaosVehicleWheel* wheel = wheelClass ? wheelClass->GetDefaultObject<UChaosVehicleWheel>() : nullptr) {
		return wheel->MaxSteerAngle;
	}
	return 0.f;
}

void AVehicleElement::ShowOptions()
{
	AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!playerController) return;
	UMeetOptionWidget* meetOption = playerController->GetMeetOptionWidget();
	if (!meetOption) return;

	AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	meetOption->RemoveName(vehicleNameOnly);
	for (int32 idx = 0; idx < cachedOptions.Num(); idx++) {
		meetOption->AddOption(frameworkActor, cachedOptions[idx], vehicleNameOnly, idx);
	}
}

void AVehicleElement::HideOptions()
{
	if (AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))) {
		if (UMeetOptionWidget* meetOption = playerController->GetMeetOptionWidget()) {
			meetOption->RemoveName(vehicleNameOnly);
		}
	}
}

void AVehicleElement::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 动态按键Context由AForeverPlayerController::BeginPlay常驻加载；这里只加跟本Pawn绑定的移动/视角Context。
	if (APlayerController* playerController = Cast<APlayerController>(NewController)) {
		if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(playerController->GetLocalPlayer())) {
			if (!bRideMode) subsystem->AddMappingContext(inputMapping, 0);
			subsystem->AddMappingContext(inputLookMapping, 0);
		}
		if (bRideMode) {
			// 搭乘：车由AI Controller继续开，这里只显示选项(比如停站时的"下车")。
			if (cachedOptions.Num() == 0 && vehicle) RefreshOptions();
			ShowOptions();
		}
		else {
			// 玩家接手驾驶：松开AI可能留下的手刹。
			SetHandbrake(false);
		}
	}
}

void AVehicleElement::PawnClientRestart()
{
	Super::PawnClientRestart();

	// APlayerController::OnPossess在PossessedBy之后才SetControlRotation(车身姿态)，所以只能在这里(其后的ClientRestart)改：
	// 车歪着视角就一直歪，翻着则上下颠倒。只保留朝向(yaw)，roll强制归零、pitch放平；之后Look只改yaw/pitch。
	if (APlayerController* playerController = Cast<APlayerController>(GetController())) {
		playerController->SetControlRotation(FRotator(-15.f, GetActorRotation().Yaw, 0.f));
	}
}

void AVehicleElement::UnPossessed()
{
	APlayerController* playerController = Cast<APlayerController>(GetController());

	// 油门/刹车/转向是持久状态，玩家失去控制后没人再调Move()，车会按最后的油门一直冲，
	// 所以这里清零油门/转向并拉满刹车。AI Controller换手(以及搭乘模式的玩家)不做：AI每帧自己写输入。
	if (vehicleMovement && playerController && !bRideMode) {
		vehicleMovement->SetThrottleInput(0.f);
		vehicleMovement->SetSteeringInput(0.f);
		vehicleMovement->SetBrakeInput(1.f);
	}

	if (playerController) {
		if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(playerController->GetLocalPlayer())) {
			subsystem->RemoveMappingContext(inputMapping);
			subsystem->RemoveMappingContext(inputLookMapping);
		}
		if (bRideMode) {
			HideOptions();
			bRideMode = false;
		}
	}

	Super::UnPossessed();
}

void AVehicleElement::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* enhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		enhancedInput->BindAction(moveAction, ETriggerEvent::Triggered, this, &AVehicleElement::Move);
		enhancedInput->BindAction(moveAction, ETriggerEvent::Completed, this, &AVehicleElement::StopMove);
		enhancedInput->BindAction(moveAction, ETriggerEvent::Canceled, this, &AVehicleElement::StopMove);
		enhancedInput->BindAction(lookAction, ETriggerEvent::Triggered, this, &AVehicleElement::Look);
		enhancedInput->BindAction(mouseLookAction, ETriggerEvent::Triggered, this, &AVehicleElement::Look);

		if (UGameInstance* gameInstance = GetGameInstance()) {
			if (UForeverKeyBindingSubsystem* keyBindings = gameInstance->GetSubsystem<UForeverKeyBindingSubsystem>()) {
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("ExitVehicle")), ETriggerEvent::Started, this, &AVehicleElement::ExitVehicle);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Handbrake")), ETriggerEvent::Started, this, &AVehicleElement::Handbrake);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Handbrake")), ETriggerEvent::Completed, this, &AVehicleElement::StopHandbrake);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Handbrake")), ETriggerEvent::Canceled, this, &AVehicleElement::StopHandbrake);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("MeetOptionUp")), ETriggerEvent::Started, this, &AVehicleElement::MeetOptionFocusUp);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("MeetOptionDown")), ETriggerEvent::Started, this, &AVehicleElement::MeetOptionFocusDown);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("MeetOptionSelect")), ETriggerEvent::Started, this, &AVehicleElement::MeetOptionSelect);
			}
		}
	}
}

void AVehicleElement::Move(const FInputActionValue& value)
{
	if (bRideMode) return;
	const FVector2D movement = value.Get<FVector2D>();

	if (movement.Y >= 0.f) {
		SetDriveInput(movement.Y, 0.f, movement.X);  // W + A/D
	} else {
		SetDriveInput(0.f, -movement.Y, movement.X); // S + A/D
	}
}

void AVehicleElement::StopMove(const FInputActionValue& value)
{
	if (bRideMode) return;
	SetDriveInput(0.f, 0.f, 0.f);
}

void AVehicleElement::Look(const FInputActionValue& value)
{
	const FVector2D look = value.Get<FVector2D>();

	if (Controller != nullptr) {
		AddControllerYawInput(look.X);
		AddControllerPitchInput(look.Y);
	}
}

void AVehicleElement::MeetOptionFocusUp()
{
	if (AForeverPlayerController* pc = Cast<AForeverPlayerController>(GetController())) {
		if (UMeetOptionWidget* meetOption = pc->GetMeetOptionWidget()) meetOption->FocusUp();
	}
}

void AVehicleElement::MeetOptionFocusDown()
{
	if (AForeverPlayerController* pc = Cast<AForeverPlayerController>(GetController())) {
		if (UMeetOptionWidget* meetOption = pc->GetMeetOptionWidget()) meetOption->FocusDown();
	}
}

void AVehicleElement::MeetOptionSelect()
{
	if (AForeverPlayerController* pc = Cast<AForeverPlayerController>(GetController())) {
		if (UMeetOptionWidget* meetOption = pc->GetMeetOptionWidget()) meetOption->ClickFocus();
	}
}

void AVehicleElement::ExitVehicle()
{
	// 搭乘模式下车走公交自己的"下车"选项(脚本alight)，Q键不处理。
	if (bRideMode) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	if (framework && framework->GetTrafficFramework()) {
		framework->GetTrafficFramework()->ExitVehicle(Cast<APlayerController>(GetController()));
	}
}

void AVehicleElement::Handbrake(const FInputActionValue& value)
{
	if (!bRideMode) SetHandbrake(true);
}

void AVehicleElement::StopHandbrake(const FInputActionValue& value)
{
	if (!bRideMode) SetHandbrake(false);
}

void AVehicleElement::PushNearbyCharacters()
{
	UWorld* world = GetWorld();
	if (!world || !carMesh || !carMesh->IsCollisionEnabled()) return;

	// 车的包围盒内所有Pawn胶囊。人的胶囊对Vehicle通道是Overlap（ForeverCharacter构造里设的），两边都不产生阻挡/物理接触，
	// 车完全不受人影响；"人被车推着走"在这里手动做：把和车身重叠的人沿最小穿透方向挪出车身，
	// 车往前开时每帧都会穿进去一点再被挪出来，效果就是被车头推着走，不弹开。
	const FBoxSphereBounds bounds = carMesh->Bounds;
	FCollisionQueryParams params(SCENE_QUERY_STAT(VehiclePushCharacters), false, this);
	TArray<FOverlapResult> overlaps;
	if (!world->OverlapMultiByObjectType(overlaps, bounds.Origin, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeBox(bounds.BoxExtent), params)) return;

	TSet<ACharacter*> handled;
	for (const FOverlapResult& overlap : overlaps) {
		UCapsuleComponent* capsule = Cast<UCapsuleComponent>(overlap.GetComponent());
		ACharacter* character = capsule ? Cast<ACharacter>(capsule->GetOwner()) : nullptr;
		if (!character || character->GetRootComponent() != capsule || handled.Contains(character)) continue;
		// 隐藏上车的玩家pawn关了碰撞（SetActorEnableCollision(false)），不能被挪。
		if (!character->GetActorEnableCollision() || !capsule->IsCollisionEnabled()) continue;
		handled.Add(character);

		FMTDResult mtd;
		if (!carMesh->ComputePenetration(mtd, capsule->GetCollisionShape(), capsule->GetComponentLocation(), capsule->GetComponentQuat())) continue;
		FVector adjust = mtd.Direction * (mtd.Distance + 1.f);
		adjust.Z = 0.f; // 只水平推：站在车顶/车边时别把人往上弹
		if (adjust.IsNearlyZero()) continue;
		character->AddActorWorldOffset(adjust, true); // 带扫掠：不会被车推进墙里
	}
}

void AVehicleElement::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!vehicle) return;

	PushNearbyCharacters();

	FVector location = GetActorLocation();
	// 线路公交的Core位姿用地图单位（Route::Update的约定，和停车场车辆的UE单位不同）；物理段由这里写回，
	// 切回时刻表时Route::Update会直接覆盖。
	const float scale = vehicle->GetRoute() ? 0.001f : 1.f;
	vehicle->SetTransform(location.X * scale, location.Y * scale, location.Z * scale, GetActorRotation().Yaw);
}

void AVehicleElement::EnsureOptionsForPlayer(APawn* player) {
	if (!vehicle || !proximityBox || !player || !proximityBox->IsOverlappingActor(player)) return;
	AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	UMeetOptionWidget* meetOption = playerController ? playerController->GetMeetOptionWidget() : nullptr;
	if (!meetOption) return;
	AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	meetOption->RemoveName(vehicleNameOnly);
	for (int32 idx = 0; idx < cachedOptions.Num(); idx++) {
		meetOption->AddOption(frameworkActor, cachedOptions[idx], vehicleNameOnly, idx);
	}
}

void AVehicleElement::RefreshOptions() {
	if (!vehicle) return;
	cachedOptions.Reset();
	for (const std::string& option : vehicle->GetOptions()) {
		cachedOptions.Add(UTF8_TO_TCHAR(option.c_str()));
	}
	if (!proximityBox) {
		BuildProximityBox(); // 之前没有脚本/选项没建盒，现在有选项了
		if (!bRideMode) return;
	}
	if (bRideMode) {
		// 搭乘中：ReplaceOptions 在名下没有按钮时是空操作（上车被撤掉后列表为空，再加"下车"就丢了），
		// 所以直接先清再加，同 ATransitVehicleElement::RefreshOptions。
		ShowOptions();
		return;
	}
	if (AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))) {
		if (UMeetOptionWidget* meetOption = playerController->GetMeetOptionWidget()) {
			AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(
				UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
			meetOption->ReplaceOptions(frameworkActor, vehicleNameOnly, cachedOptions);
		}
	}
}
