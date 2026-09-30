#include "ForeverCharacter.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"
#include "Input/ForeverKeyBindingSubsystem.h"
#include "Player/ForeverPlayerController.h"
#include "Player/ForeverWeaponComponent.h"
#include "UI/MeetOptionWidget.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

AForeverCharacter::AForeverCharacter()
{
	PrimaryActorTick.bCanEverTick = true; // 瞄准摄像机插值需要每帧跑，见Tick()

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Mesh的碰撞预设"CharacterMesh"的ObjectType同样是Pawn，对着任何Trigger(比如
	// ABuildingElement/ACitizenElement那些进入/离开检测盒)会各自独立生成一次Overlap——
	// Capsule触发一次、Mesh再触发一次，表现为同一次进出打印两条一样的消息；蒙皮网格还会随
	// 动画摆动肢体，静止不动时也可能被相邻Room盒的边界扫到，造成"明明没进房间却不停进出"的
	// 抖动。这两个bug的根源都是Mesh不该参与游戏逻辑判定的Overlap——只有Capsule(角色的
	// 真实包围体，不随动画摆动)才应该触发这些检测。
	GetMesh()->SetGenerateOverlapEvents(false);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 500.f * 10.f; // 跳跃能力增加10倍，方便从空中检视新造的城市
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = walkSpeed;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	cameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	cameraBoom->SetupAttachment(RootComponent);
	cameraBoom->TargetArmLength = defaultArmLength;
	cameraBoom->SocketOffset = defaultSocketOffset;
	cameraBoom->bUsePawnControlRotation = true;

	followCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	followCamera->SetupAttachment(cameraBoom, USpringArmComponent::SocketName);
	followCamera->bUsePawnControlRotation = false;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> meshFinder(
		TEXT("/Game/Asset/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (meshFinder.Succeeded()) {
		GetMesh()->SetSkeletalMesh(meshFinder.Object);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}

	static ConstructorHelpers::FClassFinder<UAnimInstance> animFinder(
		TEXT("/Game/Asset/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (animFinder.Succeeded()) {
		GetMesh()->SetAnimInstanceClass(animFinder.Class);
	}

	firstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	if (GetMesh()->DoesSocketExist(TEXT("head"))) {
		firstPersonCamera->SetupAttachment(GetMesh(), TEXT("head"));
	} else {
		firstPersonCamera->SetupAttachment(RootComponent);
		firstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.9f));
	}
	firstPersonCamera->bUsePawnControlRotation = true;
	firstPersonCamera->Deactivate();

	weaponComponent = CreateDefaultSubobject<UForeverWeaponComponent>(TEXT("WeaponComponent"));

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

void AForeverCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// keyBindings->GetBindingContext()这份动态Context不在这里加了——现在由
	// AForeverPlayerController::BeginPlay常驻加一份，理由是它换Pawn/换乘载具时不会跟着变，
	// 见那边的注释。这里只保留跟这个Pawn自己绑定的移动/视角Context。
	if (APlayerController* playerController = Cast<APlayerController>(NewController)) {
		if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(playerController->GetLocalPlayer())) {
			subsystem->AddMappingContext(inputMapping, 0);
			subsystem->AddMappingContext(inputLookMapping, 0);
		}
	}

	// 武器系统：被真正的玩家占有时默认配一把手枪，这样切到任何一个可操控角色(玩家自己的
	// 初始角色，或者ChangeControlChange换过去的citizen)手上都能立刻测开火/换弹/切枪，
	// 不需要额外的UI/背包流程。每次PossessedBy都会重新配一把满弹匣的手枪(不会保留上次
	// 这个角色被占有时剩下的弹药/切换到的武器)，这是MVP阶段的已知简化，见
	// ForeverWeaponComponent.md。
	if (weaponComponent) {
		weaponComponent->EquipWeapon(TEXT("weapon_pistol"));
	}
}

void AForeverCharacter::UnPossessed()
{
	if (APlayerController* playerController = Cast<APlayerController>(GetController())) {
		if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(playerController->GetLocalPlayer())) {
			subsystem->RemoveMappingContext(inputMapping);
			subsystem->RemoveMappingContext(inputLookMapping);
		}
	}

	Super::UnPossessed();
}

void AForeverCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 瞄准摄像机平滑过渡——每帧把cameraBoom的SocketOffset/TargetArmLength朝目标值(瞄准中
	// 用aimSocketOffset/aimArmLength，否则用defaultSocketOffset/defaultArmLength)插值一步，
	// 不是StartAim/StopAim里直接赋值瞬间跳变，见头文件aimTransitionSpeed注释。这个Tick()
	// 要正常跑，前提是这个实例的Actor Tick没被关掉——ACitizenElement默认对静止citizen关闭
	// Tick(省性能)，被玩家占有时会重新打开，见ACitizenElement::PossessedBy/UnPossessed。
	FVector targetSocketOffset = bIsAiming ? aimSocketOffset : defaultSocketOffset;
	float targetArmLength = bIsAiming ? aimArmLength : defaultArmLength;
	cameraBoom->SocketOffset = FMath::VInterpTo(cameraBoom->SocketOffset, targetSocketOffset, DeltaTime, aimTransitionSpeed);
	cameraBoom->TargetArmLength = FMath::FInterpTo(cameraBoom->TargetArmLength, targetArmLength, DeltaTime, aimTransitionSpeed);
}

void AForeverCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* enhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		enhancedInput->BindAction(moveAction, ETriggerEvent::Triggered, this, &AForeverCharacter::Move);
		enhancedInput->BindAction(lookAction, ETriggerEvent::Triggered, this, &AForeverCharacter::Look);
		enhancedInput->BindAction(mouseLookAction, ETriggerEvent::Triggered, this, &AForeverCharacter::Look);

		if (UGameInstance* gameInstance = GetGameInstance()) {
			if (UForeverKeyBindingSubsystem* keyBindings = gameInstance->GetSubsystem<UForeverKeyBindingSubsystem>()) {
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Jump")), ETriggerEvent::Started, this, &ACharacter::Jump);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Jump")), ETriggerEvent::Completed, this, &ACharacter::StopJumping);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("ToggleView")), ETriggerEvent::Started, this, &AForeverCharacter::ToggleCameraView);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Sprint")), ETriggerEvent::Started, this, &AForeverCharacter::StartSprint);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Sprint")), ETriggerEvent::Completed, this, &AForeverCharacter::StopSprint);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Test")), ETriggerEvent::Started, this, &AForeverCharacter::ToggleVehicle);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("MeetOptionUp")), ETriggerEvent::Started, this, &AForeverCharacter::MeetOptionFocusUp);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("MeetOptionDown")), ETriggerEvent::Started, this, &AForeverCharacter::MeetOptionFocusDown);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("MeetOptionSelect")), ETriggerEvent::Started, this, &AForeverCharacter::MeetOptionSelect);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("FireWeapon")), ETriggerEvent::Started, this, &AForeverCharacter::StartFireWeapon);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("FireWeapon")), ETriggerEvent::Completed, this, &AForeverCharacter::StopFireWeapon);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("ReloadWeapon")), ETriggerEvent::Started, this, &AForeverCharacter::ReloadWeapon);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("SwitchWeapon1")), ETriggerEvent::Started, this, &AForeverCharacter::SwitchToWeapon1);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("SwitchWeapon2")), ETriggerEvent::Started, this, &AForeverCharacter::SwitchToWeapon2);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("AimWeapon")), ETriggerEvent::Started, this, &AForeverCharacter::StartAim);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("AimWeapon")), ETriggerEvent::Completed, this, &AForeverCharacter::StopAim);
			}
		}
	}
}

void AForeverCharacter::ToggleCameraView()
{
	bIsFirstPerson = !bIsFirstPerson;

	followCamera->SetActive(!bIsFirstPerson);
	firstPersonCamera->SetActive(bIsFirstPerson);

	GetMesh()->SetOwnerNoSee(bIsFirstPerson);

	UpdateRotationMode();
}

void AForeverCharacter::UpdateRotationMode()
{
	// 第一人称、或者瞄准中(不管第几人称)，角色朝向都要跟摄像机走，不是跟移动方向走——
	// 瞄准时人应该正对着准星指向的地方，不能因为往侧面/后面移动就转向移动方向。
	bool bFaceCamera = bIsFirstPerson || bIsAiming;
	bUseControllerRotationYaw = bFaceCamera;
	GetCharacterMovement()->bOrientRotationToMovement = !bFaceCamera;
}

void AForeverCharacter::StartSprint()
{
	bIsSprinting = true;
	GetCharacterMovement()->MaxWalkSpeed = walkSpeed * sprintSpeedMultiplier;
}

void AForeverCharacter::StopSprint()
{
	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = walkSpeed;
}

void AForeverCharacter::ToggleVehicle()
{
	UForeverTrafficFrameworkComponent::RequestToggleVehicle(GetWorld(), Cast<APlayerController>(GetController()));
}

void AForeverCharacter::MeetOptionFocusUp()
{
	if (AForeverPlayerController* pc = Cast<AForeverPlayerController>(GetController())) {
		if (UMeetOptionWidget* meetOption = pc->GetMeetOptionWidget()) {
			meetOption->FocusUp();
		}
	}
}

void AForeverCharacter::MeetOptionFocusDown()
{
	if (AForeverPlayerController* pc = Cast<AForeverPlayerController>(GetController())) {
		if (UMeetOptionWidget* meetOption = pc->GetMeetOptionWidget()) {
			meetOption->FocusDown();
		}
	}
}

void AForeverCharacter::MeetOptionSelect()
{
	if (AForeverPlayerController* pc = Cast<AForeverPlayerController>(GetController())) {
		if (UMeetOptionWidget* meetOption = pc->GetMeetOptionWidget()) {
			meetOption->ClickFocus();
		}
	}
}

void AForeverCharacter::StartFireWeapon()
{
	if (weaponComponent) weaponComponent->StartFire();
}

void AForeverCharacter::StopFireWeapon()
{
	if (weaponComponent) weaponComponent->StopFire();
}

void AForeverCharacter::ReloadWeapon()
{
	if (weaponComponent) weaponComponent->Reload();
}

void AForeverCharacter::SwitchToWeapon1()
{
	if (weaponComponent) weaponComponent->EquipWeapon(TEXT("weapon_pistol"));
}

void AForeverCharacter::StartAim()
{
	bIsAiming = true;
	UpdateRotationMode();
}

void AForeverCharacter::StopAim()
{
	bIsAiming = false;
	UpdateRotationMode();
}

void AForeverCharacter::SwitchToWeapon2()
{
	if (weaponComponent) weaponComponent->EquipWeapon(TEXT("weapon_rifle"));
}

void AForeverCharacter::Move(const FInputActionValue& value)
{
	const FVector2D movement = value.Get<FVector2D>();

	if (Controller != nullptr) {
		const FRotator yawRotation(0, Controller->GetControlRotation().Yaw, 0);

		AddMovementInput(FRotationMatrix(yawRotation).GetUnitAxis(EAxis::X), movement.Y);
		AddMovementInput(FRotationMatrix(yawRotation).GetUnitAxis(EAxis::Y), movement.X);
	}
}

void AForeverCharacter::Look(const FInputActionValue& value)
{
	const FVector2D look = value.Get<FVector2D>();

	if (Controller != nullptr) {
		AddControllerYawInput(look.X);
		AddControllerPitchInput(look.Y);
	}
}
