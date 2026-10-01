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
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"
#include "Input/ForeverKeyBindingSubsystem.h"
#include "Player/ForeverPlayerController.h"
#include "Player/ForeverWeaponComponent.h"
#include "UI/MeetOptionWidget.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

#include "player/player.h"
#include "player/asset.h"

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

	// 武器/背包系统初始化：被真正的玩家占有时，左肩挂手枪+右肩挂步枪+激活左肩(出生自带
	// 一把可开火的手枪)，背上背一个容器，容器里放手枪/步枪子弹各120发——这样切到任何一个
	// 可操控角色(玩家自己的初始角色，或者ChangeControlChange换过去的citizen)手上都能立刻
	// 测装备/换弹/切枪/背包，不需要额外的UI流程。每次PossessedBy都会重新生成(不会保留
	// 上次这个角色被占有时剩下的状态)，这是MVP阶段的已知简化，照抄现有"每次重配满弹手枪"
	// 的既定惯例，见ForeverWeaponComponent.md。
	if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()))) {
		if (Player* player = framework->GetPlayer()) {
			// 先清理上一次占有(如果有)留下的状态，避免槽位已被占用导致这次装备失败——不需要
			// 额外清理weaponComponent，下面ActivateShoulderWeapon(1)里的EquipWeapon()会自动
			// 顶掉它可能残留的任何旧武器。
			if (Asset* old = player->RemoveByPath("leftShoulder")) player->DestroyAsset(old);
			if (Asset* old = player->RemoveByPath("rightShoulder")) player->DestroyAsset(old);
			if (Asset* old = player->RemoveByPath("back")) player->DestroyAsset(old);

			if (Asset* pistolAsset = player->CreateAsset("weapon_pistol", "ShoulderPistol")) {
				player->AddByPath("leftShoulder", pistolAsset);
			}
			if (Asset* rifleAsset = player->CreateAsset("weapon_rifle", "ShoulderRifle")) {
				player->AddByPath("rightShoulder", rifleAsset);
			}
			ActivateShoulderWeapon(1);

			if (Asset* bag = player->CreateAsset("asset_container", "PlayerBag")) {
				player->AddByPath("back", bag);
				if (Asset* pistolAmmo = player->CreateAsset("ammo_pistol", "PistolAmmo")) {
					pistolAmmo->SetCount(120);
					bag->AddContent(pistolAmmo);
				}
				if (Asset* rifleAmmo = player->CreateAsset("ammo_rifle", "RifleAmmo")) {
					rifleAmmo->SetCount(120);
					bag->AddContent(rifleAmmo);
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
	ActivateShoulderWeapon(1);
}

void AForeverCharacter::SwitchToWeapon2()
{
	ActivateShoulderWeapon(2);
}

void AForeverCharacter::ActivateShoulderWeapon(int32 slot)
{
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	Player* player = framework ? framework->GetPlayer() : nullptr;
	if (!player || !weaponComponent) return;

	Asset* toActivate = (slot == 1) ? player->GetLeftShoulder() : player->GetRightShoulder();
	if (!toActivate) {
		// 这个肩膀是空的——手上也应该跟着空下来，不能继续保留切换前那把武器(比如之前拿着
		// 右肩的枪，按1切到空的左肩，手上不该还握着右肩那把)，用户明确要求这个行为。手上
		// 空了之后StartAim()会因为HasWeapon()==false而拒绝瞄准。
		weaponComponent->ClearWeapon();
		if (GEngine) {
			GEngine->AddOnScreenDebugMessage(201, 3.f, FColor::Cyan, TEXT("当前武器: 无"));
		}
		return;
	}

	// 武器Asset本身留在肩膀槽位上不动，不删除/不挪走——用户明确要求"激活成手上能开火的武器
	// 不要把武器asset本身给删了，要还放在左右肩膀上"。EquipWeapon()只是让
	// ForeverWeaponComponent额外持有一份"活的"WeaponMod副本用于开火/换弹模拟，肩膀上的
	// Asset和这份"活的"副本是同一把枪的两种表示，彼此独立维护，互不删除/创建对方——
	// EquipWeapon内部自己会先销毁上一把"活的"武器(如果有)，不需要这里手动处理。
	FString activatedType = UTF8_TO_TCHAR(toActivate->GetType().c_str());
	weaponComponent->EquipWeapon(activatedType);

	// 屏幕左上角提示当前切到了哪把枪——用固定Key(不是-1)，切枪会刷新同一行，不往下堆叠。
	if (GEngine) {
		GEngine->AddOnScreenDebugMessage(201, 3.f, FColor::Cyan,
			FString::Printf(TEXT("当前武器: %s"), *activatedType));
	}
}

void AForeverCharacter::StartAim()
{
	// 手上没有激活任何武器就没法瞄准——用户明确要求："左右肩为空的时候切过去，因为没有
	// 武器，这个时候无法瞄准"。
	if (!weaponComponent || !weaponComponent->HasWeapon()) return;

	// 手里攥着背包物品(不管是不是武器，因为武器根本不会出现在这里，见"武器只挂肩膀"设计)
	// 就没法空手瞄准——和武器挂载机制彼此独立的常识性限制，逼玩家先Drop手里的东西。
	if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()))) {
		if (Player* player = framework->GetPlayer()) {
			if (player->GetRightHand() != nullptr) return;
		}
	}

	bIsAiming = true;
	UpdateRotationMode();
}

void AForeverCharacter::StopAim()
{
	bIsAiming = false;
	UpdateRotationMode();
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
