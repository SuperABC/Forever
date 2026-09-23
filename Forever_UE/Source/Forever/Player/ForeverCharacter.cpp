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
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

AForeverCharacter::AForeverCharacter()
{
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
	cameraBoom->TargetArmLength = 400.0f;
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

void AForeverCharacter::UnPossessed()
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

	bUseControllerRotationYaw = bIsFirstPerson;
	GetCharacterMovement()->bOrientRotationToMovement = !bIsFirstPerson;
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
