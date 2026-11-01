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
	BuildProximityBox();

	// 预置车辆生成时没有任何人操控/输入，物理刚体创建出来之后处于睡眠状态，不受重力影响——
	// 车停在半空也不会掉下来，直到玩家上车按下第一次油门/刹车(SetThrottleInput等内部会唤醒
	// 刚体)才会突然开始受重力下落，实测反馈"上车前悬空，踩油门才落地"。生成完立刻主动唤醒
	// 一次，让车一开局就正常落到地面上，不用等玩家来踩第一脚油门。
	if (carMesh) {
		// 重心下移：物理状态在SpawnActor时已经创建，直接改根骨刚体的COM nudge(照抄APlaneElement::BeginPlay)。
		// 放在Init而不是BeginPlay是因为这里才知道vehicle->GetCategory()。公交又高又宽，默认重心偏高，
		// AI开稍微拐一下弯就侧翻。
		FVector comOffset = busCenterOfMassOffset;
		if (comOffset.IsNearlyZero() && vehicle && vehicle->GetCategory() == "bus") {
			comOffset = FVector(0.f, 0.f, -busCenterOfMassDropZ);
		}
		if (!comOffset.IsNearlyZero()) carMesh->SetCenterOfMass(comOffset);
		carMesh->WakeAllRigidBodies();
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

	// 只烘焙一份快照，Overlap回调绝不解引用vehicle——和ACitizenElement同一套安全原则。这个
	// 时间点game_start广播早已跑完(见ForeverStoryFrameworkComponent::BroadcastGameStart对
	// 每辆车Script的广播)，vehicle->GetOptions()已经是稳定内容。
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
	// 还必须要求OtherActor是AForeverCharacter(人物)，不能是别的载具——不加这条，开着车靠近
	// 另一辆停着的车也会触发(这时候玩家当前pawn就是AVehicleElement自己，依然满足
	// OtherActor==pawn)，实测反馈"开车靠近另一辆车弹出了'上车'选项"。只有人下车状态(或者
	// 被ChangeControlChange换去控制的某个市民，同样是AForeverCharacter子类)走近才该弹选项。
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
	if (bTestNavigating) {
		bTestNavigating = false;
		if (UForeverTrafficFrameworkComponent* owner = navOwner.Get()) owner->ClearTestNavigationLine();
	}
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
	// SetSimulatePhysics(true)会把所有PhysType_Default的骨骼刚体重新打开，包括Chaos载具
	// 初始化时(FixupSkeletalMesh)特意关掉模拟的轮子刚体——它们会变成独立刚体留在原地。
	// 这里把轮子骨骼的刚体再关回去，由Chaos悬挂/动画驱动。
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

	// keyBindings->GetBindingContext()这份动态Context不在这里加了——现在由
	// AForeverPlayerController::BeginPlay常驻加一份，理由同AForeverCharacter::PossessedBy
	// 的说明：它换Pawn/换乘载具时不会跟着变。这里只保留跟这个Pawn自己绑定的移动/视角Context。
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

void AVehicleElement::UnPossessed()
{
	APlayerController* playerController = Cast<APlayerController>(GetController());

	// 油门/刹车/转向是持久状态(Move()里直接SetThrottleInput/SetBrakeInput，不是每帧由输入
	// 系统重新置0)——玩家驾驶时控制权一旦转走(不管是正常按Q下车，还是其它原因被UnPossess)，没人再调
	// Move()，上一次的油门值会一直保持，车就按原来的油门持续向前冲(实测反馈"踩着油门下车，
	// 车还在继续加速")。这里在玩家失去控制的时候清零油门/转向、拉满刹车，不管因为什么原因
	// 下车，车都应该自己刹停，不依赖玩家下车前松开W。AI Controller换手(以及搭乘模式的玩家)
	// 不做这件事：AI每帧自己写输入，这里清零反而会打断它。
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

void AVehicleElement::StartTestNavigation(const TArray<FVector>& path, Room* destination, UForeverTrafficFrameworkComponent* owner)
{
	if (path.Num() < 2 || !carMesh || GetController()) return;
	if (bTestNavigating) FinishTestNavigation(false);

	navPath = path;
	navIndex = 0;
	navDestination = destination;
	navOwner = owner;
	navBlockedSeconds = 0.f;

	// 车身本地包围盒（假设车头朝+X，Chaos车辆骨骼网格的惯例）：sweep用；最低点到原点的距离
	// 用来按地面高度反推原点高度，不依赖具体车型的原点放在轮子接地点还是车身中间。
	FBoxSphereBounds localBounds = carMesh->CalcBounds(FTransform::Identity);
	navBoxHalfExtent = localBounds.BoxExtent;
	navBoxCenterOffset = localBounds.Origin;
	navGroundOffset = FMath::Max(0.f, -(localBounds.Origin.Z - localBounds.BoxExtent.Z));

	carMesh->SetSimulatePhysics(false);
	if (vehicleMovement) {
		vehicleMovement->SetThrottleInput(0.f);
		vehicleMovement->SetSteeringInput(0.f);
		vehicleMovement->SetBrakeInput(0.f);
	}
	bTestNavigating = true;
}

void AVehicleElement::CancelTestNavigation()
{
	if (bTestNavigating) FinishTestNavigation(false);
}

void AVehicleElement::FinishTestNavigation(bool arrived)
{
	bTestNavigating = false;
	navPath.Reset();

	// 恢复成水平姿态再重新打开物理模拟，清掉残留速度，让车从当前位置自然落地。
	FRotator rotation = GetActorRotation();
	SetActorRotation(FRotator(0.f, rotation.Yaw, 0.f), ETeleportType::TeleportPhysics);
	ResumePhysics(FVector::ZeroVector);

	if (vehicle && arrived) vehicle->SetCurrentRoom(navDestination);
	if (UForeverTrafficFrameworkComponent* owner = navOwner.Get()) owner->ClearTestNavigationLine();
	navOwner = nullptr;
	navDestination = nullptr;
}

bool AVehicleElement::TraceGroundZ(const FVector& xy, float aroundZ, float& outZ) const
{
	FCollisionQueryParams params(SCENE_QUERY_STAT(VehicleTestNavGround), false, this);
	FHitResult hit;
	// 起点只比当前地面高一点：找的是"脚下这层"的地面，不是头顶的天花板或别的楼层。
	if (GetWorld()->LineTraceSingleByChannel(hit, FVector(xy.X, xy.Y, aroundZ + 150.f),
		FVector(xy.X, xy.Y, aroundZ - 400.f), ECC_Visibility, params)) {
		outZ = hit.ImpactPoint.Z;
		return true;
	}
	return false;
}

void AVehicleElement::TickTestNavigation(float deltaTime)
{
	if (navPath.Num() < 2) { FinishTestNavigation(false); return; }

	FVector location = GetActorLocation();
	FRotator rotation = GetActorRotation();

	// 推进路点：已经进到路点附近就看下一个，最后一个点单独判定到达。
	constexpr float kReachRadius = 150.f;
	constexpr float kArriveRadius = 100.f;
	while (navIndex < navPath.Num() - 1 && FVector::DistSquared2D(location, navPath[navIndex]) < kReachRadius * kReachRadius) {
		navIndex++;
	}
	const FVector target = navPath[navIndex];
	FVector2D toTarget(target.X - location.X, target.Y - location.Y);
	float distance = toTarget.Size();
	if (navIndex == navPath.Num() - 1 && distance < kArriveRadius) {
		FinishTestNavigation(true);
		return;
	}

	// 车头冲着目标方向转（转弯半径有限，不横着走）；角度差大时减速，原地也能慢慢转过去。
	float desiredYaw = FMath::RadiansToDegrees(FMath::Atan2(toTarget.Y, toTarget.X));
	float newYaw = FMath::FixedTurn(rotation.Yaw, desiredYaw, testNavTurnRate * deltaTime);
	float yawError = FMath::Abs(FMath::FindDeltaAngleDegrees(newYaw, desiredYaw));
	float speedScale = FMath::GetMappedRangeValueClamped(FVector2D(0.f, 60.f), FVector2D(1.f, 0.15f), yawError);
	float step = FMath::Min(testNavSpeed * speedScale * deltaTime, distance);

	FVector forward = FRotator(0.f, newYaw, 0.f).Vector();
	FVector newXY = location + forward * step;

	// 高度完全由地面决定：中心、前、后三条向下射线，中心定高度，前后高度差估算俯仰。
	float currentGroundZ = location.Z - navGroundOffset;
	float centerGround = currentGroundZ;
	bool hasGround = TraceGroundZ(newXY, currentGroundZ, centerGround);
	float newGroundZ = hasGround ? FMath::FInterpTo(currentGroundZ, centerGround, deltaTime, 12.f) : currentGroundZ;

	float pitch = rotation.Pitch;
	if (hasGround) {
		float half = navBoxHalfExtent.X * 0.8f;
		float frontGround, rearGround;
		if (TraceGroundZ(newXY + forward * half, centerGround, frontGround) &&
			TraceGroundZ(newXY - forward * half, centerGround, rearGround)) {
			float targetPitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(frontGround - rearGround, 2.f * half)), -35.f, 35.f);
			pitch = FMath::FInterpTo(rotation.Pitch, targetPitch, deltaTime, 8.f);
		}
	}

	FRotator newRotation(pitch, newYaw, 0.f);
	FVector newLocation(newXY.X, newXY.Y, newGroundZ + navGroundOffset + 2.f);

	// 碰撞：用车自己的碰撞通道（和物理模拟时墙/车/地形对它的阻挡一致）扫一个抬高的水平box，
	// 底面离地30，避开路面起伏；开始就嵌着东西(bStartPenetrating)不算挡，否则永远出不来。
	constexpr float kClearance = 30.f;
	FVector halfExtent(navBoxHalfExtent.X, navBoxHalfExtent.Y * 0.9f, FMath::Max(20.f, navBoxHalfExtent.Z - kClearance * 0.5f));
	auto sweepCenter = [&](const FVector& origin, float groundZ, float yaw) {
		FVector offset = FRotator(0.f, yaw, 0.f).RotateVector(FVector(navBoxCenterOffset.X, navBoxCenterOffset.Y, 0.f));
		return FVector(origin.X + offset.X, origin.Y + offset.Y, groundZ + kClearance + halfExtent.Z);
	};
	FCollisionQueryParams params(SCENE_QUERY_STAT(VehicleTestNavSweep), false, this);
	FHitResult hit;
	bool blocked = GetWorld()->SweepSingleByChannel(hit,
		sweepCenter(location, currentGroundZ, rotation.Yaw), sweepCenter(newLocation, newGroundZ, newYaw),
		FRotator(0.f, newYaw, 0.f).Quaternion(), carMesh->GetCollisionObjectType(),
		FCollisionShape::MakeBox(halfExtent), params) && !hit.bStartPenetrating;

	if (blocked) {
		// 只转车头不前进；卡住太久放弃，别永远顶着墙。
		navBlockedSeconds += deltaTime;
		SetActorRotation(FRotator(rotation.Pitch, newYaw, 0.f), ETeleportType::TeleportPhysics);
		if (navBlockedSeconds > 3.f) {
			FString hitName = hit.GetActor() ? hit.GetActor()->GetName() : TEXT("?");
			FString text = FString::Printf(TEXT("车辆导航被挡住，已放弃 (%s)"), *hitName);
			UE_LOG(LogTemp, Warning, TEXT("AVehicleElement::TickTestNavigation: %s，位置=%s"), *text, *location.ToString());
			if (GEngine) GEngine->AddOnScreenDebugMessage(9101, 8.f, FColor::Red, text);
			FinishTestNavigation(false);
		}
		return;
	}

	navBlockedSeconds = 0.f;
	SetActorLocationAndRotation(newLocation, newRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

void AVehicleElement::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bTestNavigating) TickTestNavigation(DeltaTime);

	if (!vehicle) return;

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
		BuildProximityBox(); // 原来没有脚本/选项没建盒，现在有选项了
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
