#include "Element/VehicleElement.h"

#include "traffic/vehicle.h"

#include "Framework/ForeverTrafficFrameworkComponent.h"

#include "Element/VehicleWheelFront.h"
#include "Element/VehicleWheelRear.h"
#include "Element/VehicleWheelAnimInstance.h"

#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/ForeverKeyBindingSubsystem.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

AVehicleElement::AVehicleElement()
{
	PrimaryActorTick.bCanEverTick = true;

	carMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CarMesh"));
	RootComponent = carMesh;
	carMesh->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
	// 直接写BodyInstance.bSimulatePhysics字段，不调用SetSimulatePhysics()——这个函数内部会
	// 立刻查物理材质(GetSimplePhysicalMaterial)，Actor的C++构造函数在引擎启动时构造类
	// 默认对象(CDO)阶段就会跑一次，那时GEngine还没初始化，调用会报"GEngine not
	// initialized! Cannot call this during native CDO construction"。这也是为什么要
	// 覆盖成true而不是像AWheeledVehiclePawn原版留false：这次借用的SportsCar_PhysicsAsset
	// 是给另一套"Modular Vehicle"系统准备的，底盘骨骼的PhysicsType大概率不是Simulated，
	// 组件级别这个开关会被物理资产per-bone设置覆盖，留false的话车辆能创建但完全不响应
	// 油门/转向、Z坐标也不受重力影响(FChaosVehicleManagerAsyncCallback发现ObjectState
	// 不是Dynamic直接跳过整个模拟)，见VehicleElement.md。
	carMesh->BodyInstance.bSimulatePhysics = true;
	carMesh->SetGenerateOverlapEvents(true);
	carMesh->SetCanEverAffectNavigation(false);

	// 纯C++重写FAnimInstanceProxy::Evaluate()驱动轮子滚动/转向，不需要在编辑器里手工搭
	// Animation Blueprint节点图，见VehicleWheelAnimInstance.h/.cpp。
	carMesh->SetAnimInstanceClass(UVehicleWheelAnimInstance::StaticClass());

	vehicleMovement = CreateDefaultSubobject<UChaosWheeledVehicleMovementComponent>(TEXT("VehicleMovement"));
	vehicleMovement->SetUpdatedComponent(carMesh);
	vehicleMovement->WheelSetups.SetNum(4);
	vehicleMovement->WheelSetups[0].WheelClass = UVehicleWheelFront::StaticClass();
	vehicleMovement->WheelSetups[1].WheelClass = UVehicleWheelFront::StaticClass();
	vehicleMovement->WheelSetups[2].WheelClass = UVehicleWheelRear::StaticClass();
	vehicleMovement->WheelSetups[3].WheelClass = UVehicleWheelRear::StaticClass();

	// 骨骼网格(carMesh->SkeletalMesh)/四个轮子的骨骼名(WheelSetups[i].BoneName)这里
	// 故意不赋值——它们是"因车型而异"的部分，交给继承这个类的蓝图子类在Details面板里
	// 设置。UChaosWheeledVehicleMovementComponent要求骨骼网格必须在Actor构造函数阶段
	// 就绑定好(见VehicleElement.md"骨骼网格必须在组件注册之前就绑好"一节)，蓝图子类的
	// 属性覆盖在蓝图保存时就已经烘焙进那个类自己的CDO，早于任何运行时SpawnActor调用，
	// 天然满足这个时序要求，不需要在这个基类的C++构造函数里做任何运行时数据查找。

	// EngineSetup.TorqueCurve默认是空曲线(FRuntimeFloatCurve没有任何key)——
	// FillEngineSetup()会拿这条曲线的GetValueRange()当分母做归一化，空曲线的MaxVal是0，
	// 直接除0出NaN扭矩。这里手写一条形状普通的扭矩曲线(低转速扭矩小、中段最大、高转速
	// 回落一点)，数值不是照抄某个真实发动机，只是给个能跑起来的合理默认，见
	// VehicleElement.md。
	FRichCurve* torqueCurve = vehicleMovement->EngineSetup.TorqueCurve.GetRichCurve();
	torqueCurve->AddKey(0.f, 400.f);
	torqueCurve->AddKey(1890.f, 500.f);
	torqueCurve->AddKey(5730.f, 400.f);
	vehicleMovement->EngineSetup.MaxTorque = 500.f;
	vehicleMovement->EngineSetup.MaxRPM = 5730.f;

	cameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	cameraBoom->SetupAttachment(RootComponent);
	// 车辆骨骼网格的组件原点大概率在车轮接地点附近(常见的车辆骨骼摆法)，不加偏移的话
	// 弹簧臂环绕中心就是"地面高度"而不是"车身高度"，车旋转/后退时镜头会缩进车身底盘
	// 以下(实测反馈"相机总是在车底")。这个偏移是相对车身本地坐标系的，车翻车/侧倒时
	// 会跟着车身一起转到任意朝向——偏移量给太大，翻车后这个点很容易被转到车身下方甚至
	// 地面以下，导致相机整个钻进地里(实测反馈)。这里给50cm作为一个不容易出问题的默认
	// 值，具体某个车型的车身比例差异较大(比如卡车比轿车高很多)的话，这也是个
	// EditAnywhere属性，可以在对应的蓝图子类里针对性覆盖。
	cameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	cameraBoom->TargetArmLength = 600.0f;
	cameraBoom->bUsePawnControlRotation = true;

	followCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	followCamera->SetupAttachment(cameraBoom, USpringArmComponent::SocketName);
	followCamera->bUsePawnControlRotation = false;

	// 和AForeverCharacter构造函数里指向同一份资源，两边各自持有一份ConstructorHelpers查出的
	// 引用，不是共享同一个C++对象字段。
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
	// 外观(骨骼网格/轮子骨骼名)已经在构造函数里直接读VehicleMod绑好了，不需要vehicle
	// 这个具体实例参与——这里只是记住上下车要用到的两个引用，见头文件Init()的说明。
	vehicle = inVehicle;
	previousPawn = inPreviousPawn;
}

void AVehicleElement::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	vehicle = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AVehicleElement::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (APlayerController* playerController = Cast<APlayerController>(NewController)) {
		if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(playerController->GetLocalPlayer())) {
			subsystem->AddMappingContext(inputMapping, 0);
			subsystem->AddMappingContext(inputLookMapping, 0);

			if (UGameInstance* gameInstance = GetGameInstance()) {
				if (UForeverKeyBindingSubsystem* keyBindings = gameInstance->GetSubsystem<UForeverKeyBindingSubsystem>()) {
					subsystem->AddMappingContext(keyBindings->GetBindingContext(), 0);
				}
			}
		}
	}
}

void AVehicleElement::UnPossessed()
{
	if (APlayerController* playerController = Cast<APlayerController>(GetController())) {
		if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(playerController->GetLocalPlayer())) {
			subsystem->RemoveMappingContext(inputMapping);
			subsystem->RemoveMappingContext(inputLookMapping);

			if (UGameInstance* gameInstance = GetGameInstance()) {
				if (UForeverKeyBindingSubsystem* keyBindings = gameInstance->GetSubsystem<UForeverKeyBindingSubsystem>()) {
					subsystem->RemoveMappingContext(keyBindings->GetBindingContext());
				}
			}
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
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Test")), ETriggerEvent::Started, this, &AVehicleElement::ToggleVehicle);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Handbrake")), ETriggerEvent::Started, this, &AVehicleElement::Handbrake);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Handbrake")), ETriggerEvent::Completed, this, &AVehicleElement::StopHandbrake);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Handbrake")), ETriggerEvent::Canceled, this, &AVehicleElement::StopHandbrake);
			}
		}
	}
}

void AVehicleElement::Move(const FInputActionValue& value)
{
	const FVector2D movement = value.Get<FVector2D>();

	if (movement.Y >= 0.f) {
		vehicleMovement->SetThrottleInput(movement.Y); // W
		vehicleMovement->SetBrakeInput(0.f);
	} else {
		vehicleMovement->SetThrottleInput(0.f);
		vehicleMovement->SetBrakeInput(-movement.Y);   // S
	}
	vehicleMovement->SetSteeringInput(movement.X);     // A/D
}

void AVehicleElement::StopMove(const FInputActionValue& value)
{
	vehicleMovement->SetThrottleInput(0.f);
	vehicleMovement->SetBrakeInput(0.f);
	vehicleMovement->SetSteeringInput(0.f);
}

void AVehicleElement::Look(const FInputActionValue& value)
{
	const FVector2D look = value.Get<FVector2D>();

	if (Controller != nullptr) {
		AddControllerYawInput(look.X);
		AddControllerPitchInput(look.Y);
	}
}

void AVehicleElement::ToggleVehicle()
{
	UForeverTrafficFrameworkComponent::RequestToggleVehicle(GetWorld(), Cast<APlayerController>(GetController()));
}

void AVehicleElement::Handbrake(const FInputActionValue& value)
{
	if (vehicleMovement) vehicleMovement->SetHandbrakeInput(true);
}

void AVehicleElement::StopHandbrake(const FInputActionValue& value)
{
	if (vehicleMovement) vehicleMovement->SetHandbrakeInput(false);
}

void AVehicleElement::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!vehicle) return;

	FVector location = GetActorLocation();
	vehicle->SetTransform(location.X, location.Y, location.Z, GetActorRotation().Yaw);
}
