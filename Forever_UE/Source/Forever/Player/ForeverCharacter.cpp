#include "ForeverCharacter.h"

#include "Framework/CoreGuard.h"

#include "Animation/AnimInstance.h"
#include "AnimationRuntime.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Input/ForeverKeyBindingSubsystem.h"
#include "Player/ForeverPlayerController.h"
#include "Player/ForeverThrowableComponent.h"
#include "Player/ForeverWeaponComponent.h"
#include "Player/ForeverWeaponVisualComponent.h"
#include "UI/ThrowListWidget.h"
#include "UI/MeetOptionWidget.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

#include "Element/BuildingElement.h"

#include "player/player.h"
#include "player/asset.h"
#include "map/room.h"
#include "map/map.h"
#include "Water/WaterQuery.h"
#include "Water/WaterWave.h"

AForeverCharacter::AForeverCharacter()
{
	PrimaryActorTick.bCanEverTick = true; // 瞄准摄像机插值需要每帧跑，见Tick()

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	// 和载具不互相阻挡：车是Chaos物理模拟的，人的胶囊是运动学的"无限重"，硬碰撞时人会把车顶走。
	// 改成Overlap后车完全不受人影响；人被车推走由AVehicleElement::PushNearbyCharacters手动做。
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);

	// Mesh的碰撞预设ObjectType也是Pawn：不关的话进出检测盒会触发两次Overlap，且蒙皮网格随动画摆动
	// 会被相邻Room盒边界扫到造成抖动。只让Capsule触发这些检测。
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
	UpdateMoveSpeeds(); // 含游泳(MOVE_Flying)的MaxFlySpeed/BrakingDecelerationFlying

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
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> femaleMeshFinder(
		TEXT("/Game/Asset/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	if (femaleMeshFinder.Succeeded()) femaleBodyMesh = femaleMeshFinder.Object;
	if (meshFinder.Succeeded()) {
		maleBodyMesh = meshFinder.Object;
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

	// 头像方块：挂在head骨骼socket上，和firstPersonCamera同一个判断；没有head时退化成挂在胶囊体顶部。
	// 纯装饰，不参与碰撞/overlap。项目Cube原生边长100，缩放=avatarSize/100。
	avatarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AvatarMesh"));
	if (GetMesh()->DoesSocketExist(TEXT("head"))) {
		avatarMesh->SetupAttachment(GetMesh(), TEXT("head"));
		avatarMesh->SetRelativeLocation(avatarOffset);
	} else {
		avatarMesh->SetupAttachment(RootComponent);
		avatarMesh->SetRelativeLocation(FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) + avatarOffset);
	}
	avatarMesh->SetRelativeRotation(avatarRotation);
	avatarMesh->SetRelativeScale3D(FVector(avatarSize / 100.f));
	avatarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	avatarMesh->SetGenerateOverlapEvents(false);
	avatarMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> avatarCubeFinder(TEXT("/Game/Asset/Common/Mesh/Cube.Cube"));
	if (avatarCubeFinder.Succeeded()) {
		avatarMesh->SetStaticMesh(avatarCubeFinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> avatarMaterialFinder(TEXT("/Game/Asset/Common/Material/TextureStatic.TextureStatic"));
	if (avatarMaterialFinder.Succeeded()) {
		avatarBaseMaterial = avatarMaterialFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UTexture2D> avatarTextureFinder(TEXT("/Game/Asset/Characters/Avatar/default.default"));
	if (avatarTextureFinder.Succeeded()) {
		avatarTexture = avatarTextureFinder.Object;
	}

	weaponComponent = CreateDefaultSubobject<UForeverWeaponComponent>(TEXT("WeaponComponent"));
	weaponVisualComponent = CreateDefaultSubobject<UForeverWeaponVisualComponent>(TEXT("WeaponVisualComponent"));
	throwableComponent = CreateDefaultSubobject<UForeverThrowableComponent>(TEXT("ThrowableComponent"));

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

	// 动态按键Context由AForeverPlayerController常驻添加，这里只加这个Pawn自己的移动/视角Context。
	if (APlayerController* playerController = Cast<APlayerController>(NewController)) {
		if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(playerController->GetLocalPlayer())) {
			subsystem->AddMappingContext(inputMapping, 0);
			subsystem->AddMappingContext(inputLookMapping, 0);
		}
	}

	// AIController占有（市民导航测试）不是玩家操控：不发初始装备、不开武器可视化。
	if (!Cast<APlayerController>(NewController)) return;

	// 这里不发装备：开局给什么由剧情脚本用grant_asset决定(肩膀/背包槽位挂在Player身上，跟着玩家走)。
	if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()))) {
		if (Player* player = framework->GetPlayer()) {
			// 武器可视化先开始显示，ActivateShoulderWeapon才能通知它手上武器由哪个Asset装备。
			if (weaponVisualComponent) weaponVisualComponent->BeginShowing();

			// EquipWeapon()会自动顶掉weaponComponent里可能残留的旧武器，不用先清理。
			ActivateShoulderWeapon(1);

			// 重新判定所在房间：占有前就站在房间里，占有后重叠状态没变，引擎不会补发BeginOverlap，
			// Player::currentRoom会一直为空。直接看胶囊体当前重叠的房间触发盒，不广播进出事件。
			Room* room = nullptr;
			TSet<UPrimitiveComponent*> overlaps;
			GetCapsuleComponent()->GetOverlappingComponents(overlaps);
			for (UPrimitiveComponent* component : overlaps) {
				ABuildingElement* building = component ? Cast<ABuildingElement>(component->GetOwner()) : nullptr;
				if (!building) continue;
				room = building->FindRoomOfBox(component);
				if (room) break;
			}
			player->SetCurrentRoom(room);
		}
	}
}

void AForeverCharacter::UnPossessed()
{
	// 换人/上车时肩膀上的枪跟着玩家走，旧身体上不留模型。
	if (weaponVisualComponent) weaponVisualComponent->EndShowing();

	if (APlayerController* playerController = Cast<APlayerController>(GetController())) {
		if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(playerController->GetLocalPlayer())) {
			subsystem->RemoveMappingContext(inputMapping);
			subsystem->RemoveMappingContext(inputLookMapping);
		}
	}

	// 换人操控时不能把列表和慢放留在世界里。
	if (AForeverPlayerController* controller = Cast<AForeverPlayerController>(GetController())) {
		if (UThrowListWidget* list = controller->GetThrowListWidget()) list->Close();
	}
	bThrowKeyHeld = false;

	// 换人/上车时不能把旧身体留在游泳状态。
	if (bSwimState) ExitSwimming();

	Super::UnPossessed();
}

void AForeverCharacter::BeginPlay()
{
	Super::BeginPlay();
	meshDefaultLocation = GetMesh()->GetRelativeLocation();
	meshDefaultRotation = GetMesh()->GetRelativeRotation().Quaternion();
	bMeshDefaultCaptured = true;
	boomBaseLocation = cameraBoom->GetRelativeLocation();
	standingCapsuleRadius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	standingCapsuleHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	// 构造函数里已经按默认值摆过一次；这里按(可能被子类/蓝图默认值改过的)属性再摆一遍。
	PlaceAvatar();
}

void AForeverCharacter::ApplyBodyMesh(bool bFemale)
{
	USkeletalMesh* body = bFemale ? femaleBodyMesh.Get() : maleBodyMesh.Get();
	if (!body || GetMesh()->GetSkeletalMeshAsset() == body) return;
	// 两个网格同一个骨骼，动画蓝图(AnimInstanceClass)不用换。
	GetMesh()->SetSkeletalMesh(body);
	// 头像方块挂在head骨骼上，摆放要按新网格的参考姿势重新算。
	PlaceAvatar();
}

void AForeverCharacter::PlaceAvatar()
{
	if (avatarMesh) {
		// 旋转：head骨骼在参考姿势下相对角色的朝向是骨骼自己定的(不直观)，这里把它抵消掉，让
		// avatarRotation变成相对角色本身的旋转(0=方块+X面朝角色正前方、+Z朝上)。relative满足
		// 骨骼朝向*relative=期望朝向，所以relative=骨骼朝向^-1*期望朝向。没挂在骨骼上时直接用。
		FQuat relativeRotation = avatarRotation.Quaternion();
		// 位置：avatarOffset是骨骼局部坐标；avatarOffsetInCharacter是角色方向下的微调，要按骨骼朝向转回
		// 骨骼局部坐标再加上去。
		FVector relativeLocation = avatarMesh->GetAttachSocketName().IsNone()
			? FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) + avatarOffset + avatarOffsetInCharacter
			: avatarOffset;
		USkeletalMesh* skeletalMesh = GetMesh()->GetSkeletalMeshAsset();
		FName socket = avatarMesh->GetAttachSocketName();
		if (skeletalMesh && !socket.IsNone()) {
			const FReferenceSkeleton& refSkeleton = skeletalMesh->GetRefSkeleton();
			int32 boneIndex = refSkeleton.FindBoneIndex(socket);
			if (boneIndex != INDEX_NONE) {
				FQuat boneInComponent = FAnimationRuntime::GetComponentSpaceTransformRefPose(refSkeleton, boneIndex).GetRotation();
				FQuat boneInActor = GetMesh()->GetRelativeRotation().Quaternion() * boneInComponent;
				relativeRotation = boneInActor.Inverse() * relativeRotation;
				relativeLocation += boneInActor.Inverse().RotateVector(avatarOffsetInCharacter);
			}
		}
		avatarMesh->SetRelativeLocation(relativeLocation);
		avatarMesh->SetRelativeRotation(relativeRotation);
		avatarMesh->SetRelativeScale3D(FVector(avatarSize / 100.f));
		// 头像贴图：Avatar材质还没建(avatarBaseMaterial为空)时保留Cube自带材质，不报错。
		if (avatarBaseMaterial) {
			// 动态材质实例只建一次(换身体网格会重复调用PlaceAvatar，贴图也要沿用)。
			if (!avatarMaterial || avatarMaterial->Parent != avatarBaseMaterial) {
				avatarMaterial = UMaterialInstanceDynamic::Create(avatarBaseMaterial, this);
			}
			if (avatarTexture) avatarMaterial->SetTextureParameterValue(TEXT("Texture"), avatarTexture);
			avatarMesh->SetMaterial(0, avatarMaterial);
		}
	}
}

void AForeverCharacter::SetAvatarTexture(UTexture2D* texture)
{
	if (!texture) return;
	// avatarTexture是UPROPERTY，运行时建出来的贴图靠它保持存活。
	avatarTexture = texture;
	if (avatarMaterial) avatarMaterial->SetTextureParameterValue(TEXT("Texture"), texture);
}

void AForeverCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	CORE_GUARD_BEGIN("AForeverCharacter::Tick")

	// 长按3：按住的真实时间到阈值就弹出投掷物选择列表。
	if (bThrowKeyHeld && GetWorld()->GetRealTimeSeconds() - throwKeyPressTime >= longPressThreshold) {
		OpenThrowList();
	}

	UpdateWaterState(DeltaTime);

	// 第一人称相机抬高：相机挂在head骨骼上，偏移要按骨骼朝向把"世界向上"转成骨骼局部坐标。
	if (bIsFirstPerson) {
		if (GetMesh()->DoesSocketExist(TEXT("head"))) {
			const FTransform socket = GetMesh()->GetSocketTransform(TEXT("head"), RTS_World);
			firstPersonCamera->SetRelativeLocation(socket.InverseTransformVectorNoScale(FVector(0.f, 0.f, firstPersonCameraRaise)));
		} else {
			firstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.9f + firstPersonCameraRaise));
		}
	}

	// 瞄准摄像机平滑过渡。依赖Actor Tick没被关掉：ACitizenElement默认关闭Tick，被玩家占有时由PossessedBy重新打开。
	FVector targetSocketOffset = bIsAiming ? aimSocketOffset : defaultSocketOffset;
	float targetArmLength = bIsAiming ? aimArmLength : defaultArmLength;
	cameraBoom->SocketOffset = FMath::VInterpTo(cameraBoom->SocketOffset, targetSocketOffset, DeltaTime, aimTransitionSpeed);
	cameraBoom->TargetArmLength = FMath::FInterpTo(cameraBoom->TargetArmLength, targetArmLength, DeltaTime, aimTransitionSpeed);
	CORE_GUARD_END
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
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Jump")), ETriggerEvent::Started, this, &AForeverCharacter::JumpPressed);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Jump")), ETriggerEvent::Completed, this, &AForeverCharacter::JumpReleased);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("ToggleView")), ETriggerEvent::Started, this, &AForeverCharacter::ToggleCameraView);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Sprint")), ETriggerEvent::Started, this, &AForeverCharacter::StartSprint);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Sprint")), ETriggerEvent::Completed, this, &AForeverCharacter::StopSprint);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("MeetOptionUp")), ETriggerEvent::Started, this, &AForeverCharacter::MeetOptionFocusUp);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("MeetOptionDown")), ETriggerEvent::Started, this, &AForeverCharacter::MeetOptionFocusDown);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("MeetOptionSelect")), ETriggerEvent::Started, this, &AForeverCharacter::MeetOptionSelect);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("FireWeapon")), ETriggerEvent::Started, this, &AForeverCharacter::StartFireWeapon);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("FireWeapon")), ETriggerEvent::Completed, this, &AForeverCharacter::StopFireWeapon);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("ReloadWeapon")), ETriggerEvent::Started, this, &AForeverCharacter::ReloadWeapon);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("SwitchWeapon1")), ETriggerEvent::Started, this, &AForeverCharacter::SwitchToWeapon1);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("SwitchWeapon2")), ETriggerEvent::Started, this, &AForeverCharacter::SwitchToWeapon2);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("ThrowItem")), ETriggerEvent::Started, this, &AForeverCharacter::ThrowKeyPressed);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("ThrowItem")), ETriggerEvent::Completed, this, &AForeverCharacter::ThrowKeyReleased);
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
	// 第一人称里网格对自己不可见(OwnerNoSee)，会被当成"没渲染"；若动画只在渲染时才更新，过1~2秒头骨(第一人称相机挂在上面)
	// 就停止摆动，武器相对镜头的步伐起伏消失。强制一直更新姿势和骨骼。
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	UpdateRotationMode();
}

void AForeverCharacter::UpdateRotationMode()
{
	// 第一人称或瞄准中，角色朝向跟摄像机走而不是移动方向。
	bool bFaceCamera = bIsFirstPerson || bIsAiming;
	bUseControllerRotationYaw = bFaceCamera;
	// 游泳/潜水时偏航朝向自己算(UpdateSwimBody)：引擎在Flying模式会让胶囊体跟着加速度方向俯仰。
	GetCharacterMovement()->bOrientRotationToMovement = !bFaceCamera && !bSwimState;
}

void AForeverCharacter::StartSprint()
{
	bIsSprinting = true;
	UpdateMoveSpeeds();
}

void AForeverCharacter::StopSprint()
{
	bIsSprinting = false;
	UpdateMoveSpeeds();
}

void AForeverCharacter::UpdateMoveSpeeds()
{
	UCharacterMovementComponent* movement = GetCharacterMovement();
	movement->MaxWalkSpeed = bIsSprinting ? walkSpeed * sprintSpeedMultiplier : walkSpeed;
	// 游泳借用MOVE_Flying，速度由MaxFlySpeed控制；冲刺键=快速游泳。
	movement->MaxFlySpeed = walkSpeed * swimSpeedMultiplier * (bIsSprinting ? swimSprintMultiplier : 1.f);
	movement->BrakingDecelerationFlying = swimBrakingDeceleration;
}

void AForeverCharacter::JumpPressed()
{
	if (bSwimState) {
		// 游泳(水面)没有跳跃；潜水时空格=上浮。
		bSwimUpHeld = bDiveState;
		return;
	}
	Jump();
}

void AForeverCharacter::JumpReleased()
{
	bSwimUpHeld = false;
	StopJumping();
}

bool AForeverCharacter::QueryWater(const FVector& worldLocation, float& outSurfaceZ, float& outBedZ, float* outBaseSurfaceZ) const
{
	AForeverFrameworkActor* framework = cachedFramework.Get();
	if (!framework) {
		framework = Cast<AForeverFrameworkActor>(UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
		cachedFramework = framework;
	}
	Map* map = framework ? framework->GetMap() : nullptr;
	// 在地形表面以下(海底隧道里)不算在水中：格子有水但人在地下，不能入水(否则一进隧道就被浮力顶飞)。
	if (ForeverWater::IsBelowTerrain(map, worldLocation)) return false;
	// 格子范围/水位/波浪/水底的计算和船的浮力共用ForeverWater::QuerySurface(Water/WaterQuery.h)。
	return ForeverWater::QuerySurface(map, worldLocation, GetWorld()->GetTimeSeconds(), outSurfaceZ, outBedZ, outBaseSurfaceZ);
}

void AForeverCharacter::UpdateWaterState(float DeltaTime)
{
	UCharacterMovementComponent* movement = GetCharacterMovement();

	// 只有玩家操控的角色游泳；市民走Core导航，不会下水。
	float surfaceZ = 0.f, bedZ = 0.f, baseSurfaceZ = 0.f;
	const bool bInWater = IsPlayerControlled() && QueryWater(GetActorLocation(), surfaceZ, bedZ, &baseSurfaceZ);
	if (!bInWater) {
		if (bSwimState) ExitSwimming();
		bIsSwimming = false;
		bIsDiving = false;
		bIsUnderwater = false;
		bIsFastSwimming = false;
		UpdateSwimBody(DeltaTime);
		return;
	}

	const FVector location = GetActorLocation();
	// 入水/出水阈值按站立身高算(游泳时胶囊体被缩短了，不能读当前值)。
	const float halfHeight = standingCapsuleHalfHeight * GetCapsuleComponent()->GetShapeScale();
	const float fullHeight = halfHeight * 2.f;
	// 入水/出水阈值用不带波浪的水位：浪是几十厘米的上下起伏，会让阈值边缘来回抖。
	const float feetSubmerge = baseSurfaceZ - (location.Z - halfHeight); // 脚底在水面下多深
	const float bedDepth = baseSurfaceZ - bedZ;                          // 水底到水面的深度

	if (!bSwimState) {
		// 只有脚底淹没到enterRatio×身高才入水(缓坡上岸要退到exitRatio×身高以下的水深才算出水)。
		if (feetSubmerge >= swimEnterDepthRatio * fullHeight
			&& (movement->IsMovingOnGround() || movement->IsFalling())) {
			EnterSwimming();
		}
	} else if (!bDiveState && bedDepth < swimExitDepthRatio * fullHeight) {
		// 出水只在游泳状态判：潜水一定在深水里。
		ExitSwimming();
	}

	if (bSwimState) {
		// 引擎可能因为别的原因把模式切走(如别的系统SetMovementMode)，保持游泳模式。
		if (movement->MovementMode != MOVE_Flying) movement->SetMovementMode(MOVE_Flying);

		const bool bHasInput = !GetLastMovementInputVector().IsNearlyZero();
		const FVector viewDirection = Controller ? Controller->GetControlRotation().Vector() : GetActorForwardVector();
		// 下潜意图：按住冲刺并且视线朝水下(与竖直向下的点积 > 阈值)。
		const bool bDiveIntent = bIsSprinting && FVector::DotProduct(viewDirection, FVector::DownVector) > swimDiveLookDot;

		const float floatZ = surfaceZ + swimFloatOffset; // 浮在水面时胶囊体中心的高度(原地/游动同一个高度，姿势高度差在动画里补)
		const float centerZ = location.Z;

		if (!bDiveState) {
			if (bDiveIntent) SetDiving(true);
		} else if (!bDiveIntent && centerZ >= floatZ - 10.f) {
			// 回到水面且不再满足下潜条件：回游泳。
			SetDiving(false);
		}

		if (bDiveState) {
			if (bSwimUpHeld) AddMovementInput(FVector::UpVector, 1.f);
			FVector velocity = movement->Velocity;
			// 没有任何输入时按swimBuoyancySpeed上浮(默认0=原地悬停，不自动上浮)。
			if (swimBuoyancySpeed > 0.f && !bHasInput && !bSwimUpHeld && centerZ < floatZ) velocity.Z = FMath::Max(velocity.Z, swimBuoyancySpeed);
			// 不能浮出水面太多：向上的速度不超过"到浮起位置的距离/帧时间"。
			velocity.Z = FMath::Min(velocity.Z, (floatZ - centerZ) / FMath::Max(DeltaTime, UE_KINDA_SMALL_NUMBER));
			movement->Velocity = velocity;
		} else {
			// 游泳：竖直方向一帧内到浮起位置(不会下潜，不做平滑，姿势切换时高度直接跳到位)。
			FVector velocity = movement->Velocity;
			velocity.Z = (floatZ - centerZ) / FMath::Max(DeltaTime, UE_KINDA_SMALL_NUMBER);
			movement->Velocity = velocity;
		}
	}

	// 水的阻力：超过游泳最大速度的部分(冲刺入水带进来的速度)快速衰减。引擎Flying模式的摩擦很小，不处理会高速滑很久。
	if (bSwimState) {
		FVector velocity = movement->Velocity;
		const float maxSpeed = movement->MaxFlySpeed;
		const float horizontalSpeed = velocity.Size2D();
		if (horizontalSpeed > maxSpeed && horizontalSpeed > UE_KINDA_SMALL_NUMBER) {
			const float newSpeed = FMath::FInterpTo(horizontalSpeed, maxSpeed, DeltaTime, swimOverspeedDecay);
			velocity.X *= newSpeed / horizontalSpeed;
			velocity.Y *= newSpeed / horizontalSpeed;
			movement->Velocity = velocity;
		}
	}

	const float headZ = GetMesh()->DoesSocketExist(TEXT("head"))
		? GetMesh()->GetSocketLocation(TEXT("head")).Z
		: location.Z + halfHeight;
	bIsUnderwater = headZ < surfaceZ;
	bIsSwimming = bSwimState;
	bIsDiving = bSwimState && bDiveState;
	bIsFastSwimming = bSwimState && bIsSprinting;
	bIsSwimMoving = bSwimState && !GetLastMovementInputVector().IsNearlyZero();
	UpdateSwimBody(DeltaTime);
}

void AForeverCharacter::EnterSwimming()
{
	bSwimState = true;
	bDiveState = false;
	// 缩短胶囊体：竖直的长胶囊体下端比趴着的身体深得多，会在到出水深度之前就顶住岸坡。以中心为基准缩，网格/相机/浮高不受影响。
	if (!bCapsuleShrunk) {
		GetCapsuleComponent()->SetCapsuleSize(standingCapsuleRadius, swimCapsuleHalfHeight);
		bCapsuleShrunk = true;
	}
	// 游泳时第三人称弹簧臂连接点从胶囊体中心抬到头部，出水恢复。
	cameraBoom->SetRelativeLocation(boomBaseLocation + FVector(0.f, 0.f, cameraBoomHeight));
	StopJumping();
	UpdateMoveSpeeds();
	UpdateRotationMode();
	SetSwimCameraLimit(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
}

void AForeverCharacter::ExitSwimming()
{
	bSwimState = false;
	bDiveState = false;
	bSwimUpHeld = false;
	// 恢复站立胶囊体，同时整体上移半高的差值，保持下端位置不变，免得嵌进岸坡。
	if (bCapsuleShrunk) {
		const float currentHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		GetCapsuleComponent()->SetCapsuleSize(standingCapsuleRadius, standingCapsuleHalfHeight);
		AddActorWorldOffset(FVector(0.f, 0.f, (standingCapsuleHalfHeight - currentHalfHeight) * GetCapsuleComponent()->GetShapeScale()), false, nullptr, ETeleportType::TeleportPhysics);
		bCapsuleShrunk = false;
	}
	cameraBoom->SetRelativeLocation(boomBaseLocation);
	bIsSwimming = false;
	bIsDiving = false;
	bIsFastSwimming = false;
	UpdateRotationMode();
	SetSwimCameraLimit(false);
	// 切回Falling：脚下有地面时引擎下一帧自动落地变回Walking。
	UCharacterMovementComponent* movement = GetCharacterMovement();
	if (movement->MovementMode == MOVE_Flying) movement->SetMovementMode(MOVE_Falling);
}

void AForeverCharacter::SetDiving(bool bDive)
{
	if (bDiveState == bDive) return;
	bDiveState = bDive;
	if (!bDive) bSwimUpHeld = false;
	// 游泳时镜头不能朝上(不进入水下)，潜水解锁全视角。
	SetSwimCameraLimit(!bDive);
}

void AForeverCharacter::SetSwimCameraLimit(bool bLimit)
{
	APlayerController* playerController = Cast<APlayerController>(GetController());
	if (!playerController || !playerController->PlayerCameraManager) return;
	APlayerCameraManager* cameraManager = playerController->PlayerCameraManager;
	if (bLimit && !bCameraLimited) {
		savedViewPitchMax = cameraManager->ViewPitchMax;
		cameraManager->ViewPitchMax = 0.f; // 水平为止，不能再抬头
		bCameraLimited = true;
	} else if (!bLimit && bCameraLimited) {
		cameraManager->ViewPitchMax = savedViewPitchMax;
		bCameraLimited = false;
	}
}

void AForeverCharacter::UpdateSwimBody(float DeltaTime)
{
	if (!bMeshDefaultCaptured) return;

	// 偏航：胶囊体只转偏航(不跟加速度俯仰)。有水平输入就朝输入方向转；面朝镜头模式交给引擎。
	if (bSwimState && !bUseControllerRotationYaw) {
		FVector input = GetLastMovementInputVector();
		input.Z = 0.f;
		if (!input.IsNearlyZero()) {
			const FRotator current = GetActorRotation();
			const FRotator target(0.f, input.Rotation().Yaw, 0.f);
			SetActorRotation(FMath::RInterpConstantTo(current, target, DeltaTime, GetCharacterMovement()->RotationRate.Yaw));
		}
	}

	// 身体俯仰：潜水=镜头俯仰×跟随比例，上浮再加一个向上转的分量；其余状态回正。
	float targetPitch = 0.f;
	if (bSwimState && bDiveState && Controller) {
		const float cameraPitch = FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch);
		targetPitch = cameraPitch * diveBodyPitchFollow;
		if (bSwimUpHeld) targetPitch += diveRiseBodyPitch;
		targetPitch = FMath::Clamp(targetPitch, -80.f, 80.f);
	}
	bodyPitch = FMath::FInterpTo(bodyPitch, targetPitch, DeltaTime, diveBodyPitchInterpSpeed);
	if (FMath::IsNearlyZero(bodyPitch, 0.05f) && targetPitch == 0.f) bodyPitch = 0.f;

	// 绕胶囊体中心转网格(网格原点在脚底，要把位置一起绕中心转)，胶囊体本身保持竖直。
	if (bodyPitch == 0.f) {
		// 没俯仰时只在网格偏离默认姿态(刚结束潜水)时回正一次，平时不动它。
		if (!GetMesh()->GetRelativeLocation().Equals(meshDefaultLocation, 0.01f)) {
			GetMesh()->SetRelativeLocationAndRotation(meshDefaultLocation, meshDefaultRotation);
		}
		return;
	}
	const FQuat pitchQuat(FVector::RightVector, FMath::DegreesToRadians(-bodyPitch));
	GetMesh()->SetRelativeLocationAndRotation(pitchQuat.RotateVector(meshDefaultLocation), pitchQuat * meshDefaultRotation);
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
	// 激活的是投掷物：左键是扔出去(瞄准中才生效，一次一个，不连发)，不走武器开火。
	if (throwableComponent && throwableComponent->IsActive()) {
		throwableComponent->Throw();
		return;
	}
	if (weaponComponent) weaponComponent->StartFire();
}

void AForeverCharacter::ThrowKeyPressed()
{
	// 列表开着时的"按下"不是新的一次按键(打开列表切换输入模式会清空按键状态，键还按着时系统的
	// 按键重复会让Pawn收到假的"按下")，忽略，否则会反复触发长按，列表一闪一闪。
	if (AForeverPlayerController* controller = Cast<AForeverPlayerController>(GetController())) {
		if (UThrowListWidget* list = controller->GetThrowListWidget()) {
			if (list->IsOpen()) return;
		}
	}

	throwKeyPressTime = GetWorld()->GetRealTimeSeconds();
	bThrowKeyHeld = true;
	bSwallowThrowRelease = false;
}

void AForeverCharacter::ThrowKeyReleased()
{
	bThrowKeyHeld = false;

	// 列表开着时收到的"松开"是打开列表切换输入模式时按键状态被清空造成的假事件，不是真的松开，
	// 不能据此关闭列表；真正的松开由列表控件自己的NativeOnKeyUp处理(UI Only下它才收得到按键)。
	if (AForeverPlayerController* controller = Cast<AForeverPlayerController>(GetController())) {
		if (UThrowListWidget* list = controller->GetThrowListWidget()) {
			if (list->IsOpen()) return;
		}
	}

	// 这次按键已经弹出过列表(长按)：松开不是短按。
	if (bSwallowThrowRelease) {
		bSwallowThrowRelease = false;
		return;
	}
	if (throwableComponent) throwableComponent->CycleThrowable();
}

void AForeverCharacter::OpenThrowList()
{
	// 不管列表有没有成功打开(世界被别的系统冻结时会打不开)，这次按键都算长按，松开时不切换。
	bThrowKeyHeld = false;
	bSwallowThrowRelease = true;

	AForeverPlayerController* controller = Cast<AForeverPlayerController>(GetController());
	UThrowListWidget* list = controller ? controller->GetThrowListWidget() : nullptr;
	if (!list || !throwableComponent) return;
	list->Open(throwableComponent->GetThrowableList(), throwableComponent->GetActiveType());
}

bool AForeverCharacter::IsAimingThrow() const
{
	return bIsAiming && throwableComponent && throwableComponent->IsActive();
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

	// 切回肩上的武器：退出投掷状态；原来是投掷瞄准的话一并结束瞄准。
	if (throwableComponent && throwableComponent->IsActive()) {
		throwableComponent->Deactivate();
		StopAim();
	}

	activeShoulderSlot = slot;
	Asset* toActivate = (slot == 1) ? player->GetLeftShoulder() : player->GetRightShoulder();
	if (weaponVisualComponent) weaponVisualComponent->NotifyActivated(slot, toActivate);
	if (!toActivate) {
		// 这个肩膀是空的：手上跟着空下来，不保留切换前那把；StartAim()会因HasWeapon()==false拒绝瞄准。
		weaponComponent->ClearWeapon();
		if (GEngine) {
			GEngine->AddOnScreenDebugMessage(201, 3.f, FColor::Cyan, TEXT("当前武器: 无"));
		}
		return;
	}

	// 武器Asset留在肩膀槽位上不动，EquipWeapon只让组件额外持有一份"活的"WeaponMod副本(会先销毁旧副本)。
	// 资产id(asset_xxx)和武器id(weapon_xxx)分属两个concept，由资产自己声明对应哪把武器。
	FString activatedType = UTF8_TO_TCHAR(toActivate->GetWeaponType().c_str());
	if (weaponComponent->EquipWeapon(activatedType)) {
		// 弹匣余量跟着这把枪的Asset走：绑定槽位(之后开火/换弹写回)，Asset记着余量就恢复，否则保持满弹匣。
		weaponComponent->SetSourceSlot(slot);
		if (toActivate->GetMagazineAmmo() >= 0) weaponComponent->SetCurrentAmmo(toActivate->GetMagazineAmmo());
	}

	// 固定Key，切枪刷新同一行。
	if (GEngine) {
		GEngine->AddOnScreenDebugMessage(201, 3.f, FColor::Cyan,
			FString::Printf(TEXT("当前武器: %s"), *activatedType));
	}
}

void AForeverCharacter::StartAim()
{
	// 激活的是投掷物：用CanThrow()判断，不要求HasWeapon()，也没有武器的右手限制。
	if (throwableComponent && throwableComponent->IsActive()) {
		throwableComponent->Refresh(); // 背包没有变化事件，开始瞄准前重新核对
		if (!throwableComponent->IsActive() || !throwableComponent->CanThrow()) return;
		bIsAiming = true;
		UpdateRotationMode();
		return;
	}

	// 手上没有激活任何武器就没法瞄准。
	if (!weaponComponent || !weaponComponent->HasWeapon()) return;

	// 右手攥着背包物品时没法瞄准(武器只挂肩膀，不会在手里)，要先放下。
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

		// 潜水时前进方向带俯仰：抬头往上游，低头往下潜；水面游泳只在水平面内。
		const FRotator forwardRotation = bDiveState ? Controller->GetControlRotation() : yawRotation;
		AddMovementInput(FRotationMatrix(forwardRotation).GetUnitAxis(EAxis::X), movement.Y);
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

void AForeverCharacter::RestoreWeaponState(int32 slot, int32 magazineAmmo)
{
	if (slot != 1 && slot != 2) return;
	ActivateShoulderWeapon(slot);
	if (weaponComponent && weaponComponent->HasWeapon() && magazineAmmo >= 0) {
		weaponComponent->SetCurrentAmmo(magazineAmmo);
	}
}
