#include "Element/TransitVehicleElement.h"

#include "traffic/vehicle.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Player/ForeverPlayerController.h"
#include "Player/ForeverCharacter.h"
#include "UI/MeetOptionWidget.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/ForeverKeyBindingSubsystem.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

// 和ForeverTrafficFrameworkComponent.cpp的TRAFFIC_WORLD_SCALE同一个值，这次按本文件既有
// 约定各自维护一份(项目既有约定，见ForeverTrafficFrameworkComponent.cpp顶部注释)。
#define TRANSIT_WORLD_SCALE 1000.f

// Cube.Cube默认边长100cm，缩放系数=目标尺寸/100。
#define TRANSIT_CUBE_DEFAULT_SIZE 100.f

// 搭乘触发盒比模型包围盒外扩的量(UE单位)：水平方向够站在车门外，竖直方向小一点。
#define TRANSIT_BOARD_BOX_MARGIN_XY 150.f
#define TRANSIT_BOARD_BOX_MARGIN_Z 50.f

// 搭乘视角的弹簧臂长度。
#define TRANSIT_CAMERA_ARM_LENGTH 1100.f

ATransitVehicleElement::ATransitVehicleElement()
{
	PrimaryActorTick.bCanEverTick = true;

	bodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	RootComponent = bodyMesh;

	// 运动学显示物，外观本身不参与任何碰撞查询(物理碰撞不跟着它走)；可搭乘的车另外建一个只
	// overlap的触发盒(BuildBoardingComponents)。
	bodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bodyMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> cubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (cubeFinder.Succeeded()) {
		bodyMesh->SetStaticMesh(cubeFinder.Object);
	}

	skeletalBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalBody"));
	skeletalBody->SetupAttachment(RootComponent);
	skeletalBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	skeletalBody->SetCanEverAffectNavigation(false);
	skeletalBody->SetVisibility(false);

	// 搭乘视角：弹簧臂+相机，和AVehicleElement同一套(臂跟随控制器旋转，鼠标转视角)。位置/臂长
	// 在BuildBoardingComponents里按模型包围盒调整。
	cameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	cameraBoom->SetupAttachment(RootComponent);
	cameraBoom->TargetArmLength = TRANSIT_CAMERA_ARM_LENGTH;
	cameraBoom->bUsePawnControlRotation = true;

	followCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	followCamera->SetupAttachment(cameraBoom, USpringArmComponent::SocketName);
	followCamera->bUsePawnControlRotation = false;

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

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> inputLookMappingFinder(
		TEXT("/Game/Blueprint/Player/Input/IMC_MouseLook.IMC_MouseLook"));
	if (inputLookMappingFinder.Succeeded()) {
		inputLookMapping = inputLookMappingFinder.Object;
	}
}

void ATransitVehicleElement::Init(Vehicle* inVehicle)
{
	vehicle = inVehicle;
	if (!vehicle || !bodyMesh) return;

	// transitMeshPath非空时优先加载真实网格——模型自己的尺寸就是对的，不再按sizeX/Y/Z缩放立方体。
	// 先按任意资产加载再判断是骨骼网格还是静态网格(直接LoadObject<T>在类型不符时会刷警告)。
	// 加载失败(mod的Plugin没挂载上/路径打错)时退化回立方体占位，和留空时的默认行为一致，
	// 见vehicle_mod.h::transitMeshPath的说明。
	const std::string& meshPath = vehicle->GetTransitMeshPath();
	if (!meshPath.empty()) {
		UObject* asset = StaticLoadObject(UObject::StaticClass(), nullptr, UTF8_TO_TCHAR(meshPath.c_str()));
		float meshScale = vehicle->GetMeshScale();
		// 不能在这里直接SetRelativeRotation——bodyMesh是RootComponent，Tick()里
		// SetActorLocationAndRotation直接设的就是RootComponent的世界旋转，这里设的
		// 任何值都会被下一次Tick覆盖掉。缓存这个偏移，叠进Tick()每帧算的朝向里。
		meshYawOffsetDegrees = vehicle->GetMeshYawOffsetDegrees();

		if (USkeletalMesh* skeletal = Cast<USkeletalMesh>(asset)) {
			// 骨骼网格挂在root下面当子组件，缩放只作用在它自己身上(root保持1倍，触发盒/相机才不会被缩)。
			bodyMesh->SetStaticMesh(nullptr);
			skeletalBody->SetSkeletalMesh(skeletal);
			skeletalBody->SetRelativeScale3D(FVector(meshScale, meshScale, meshScale));
			skeletalBody->SetVisibility(true);

			const FBoxSphereBounds bounds = skeletal->GetBounds();
			UE_LOG(LogTemp, Log, TEXT("ATransitVehicleElement::Init: %s 骨骼网格包围盒 origin=%s extent=%s (x%.2f)"),
				UTF8_TO_TCHAR(vehicle->GetName().c_str()), *bounds.Origin.ToString(), *bounds.BoxExtent.ToString(), meshScale);
			BuildBoardingComponents(bounds);
			return;
		}
		if (UStaticMesh* mesh = Cast<UStaticMesh>(asset)) {
			bodyMesh->SetStaticMesh(mesh);
			bodyMesh->SetWorldScale3D(FVector(meshScale, meshScale, meshScale));
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("ATransitVehicleElement::Init: 加载transitMeshPath失败: %s，退化为立方体占位"),
			UTF8_TO_TCHAR(meshPath.c_str()));
		meshYawOffsetDegrees = 0.f;
	}

	float sizeX, sizeY, sizeZ;
	vehicle->GetSize(sizeX, sizeY, sizeZ);
	bodyMesh->SetWorldScale3D(FVector(
		sizeX / TRANSIT_CUBE_DEFAULT_SIZE,
		sizeY / TRANSIT_CUBE_DEFAULT_SIZE,
		sizeZ / TRANSIT_CUBE_DEFAULT_SIZE));
}

void ATransitVehicleElement::BuildBoardingComponents(const FBoxSphereBounds& localBounds)
{
	if (!vehicle || !vehicle->IsBoardable()) return;

	float meshScale = vehicle->GetMeshScale();
	FVector center = localBounds.Origin * meshScale;
	FVector extent = localBounds.BoxExtent * meshScale;

	UBoxComponent* box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
	box->SetBoxExtent(extent + FVector(TRANSIT_BOARD_BOX_MARGIN_XY, TRANSIT_BOARD_BOX_MARGIN_XY, TRANSIT_BOARD_BOX_MARGIN_Z));
	box->SetRelativeLocation(center);
	box->SetCollisionProfileName(TEXT("Trigger"));
	box->SetupAttachment(RootComponent);
	box->OnComponentBeginOverlap.AddDynamic(this, &ATransitVehicleElement::OnOverlapBegin);
	box->OnComponentEndOverlap.AddDynamic(this, &ATransitVehicleElement::OnOverlapEnd);
	box->RegisterComponent();
	proximityBox = box;

	// 相机绕车顶中心转。
	cameraBoom->SetRelativeLocation(center + FVector(0.f, 0.f, extent.Z));

	BakeOptions();
	bBoardable = true;
}

void ATransitVehicleElement::BakeOptions()
{
	if (!vehicle) return;
	vehicleNameOnly = UTF8_TO_TCHAR(vehicle->GetName().c_str());
	cachedOptions.Reset();
	for (const std::string& option : vehicle->GetOptions()) {
		cachedOptions.Add(UTF8_TO_TCHAR(option.c_str()));
	}
}

void ATransitVehicleElement::ShowOptions()
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

void ATransitVehicleElement::HideOptions()
{
	if (AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))) {
		if (UMeetOptionWidget* meetOption = playerController->GetMeetOptionWidget()) {
			meetOption->RemoveName(vehicleNameOnly);
		}
	}
}

void ATransitVehicleElement::RefreshOptions()
{
	if (!bBoardable) return;
	BakeOptions();
	if (bPlayerInBox || bAboard) ShowOptions();
}

void ATransitVehicleElement::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 和AVehicleElement::OnOverlapBegin同一条过滤：只有玩家当前控制的AForeverCharacter(人物)走近
	// 才弹选项，别的载具/路人经过不算。
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn || OtherActor == this || !Cast<AForeverCharacter>(OtherActor)) return;

	bPlayerInBox = true;
	ShowOptions();
}

void ATransitVehicleElement::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn || OtherActor == this || !Cast<AForeverCharacter>(OtherActor)) return;

	bPlayerInBox = false;
	if (!bAboard) HideOptions();
}

void ATransitVehicleElement::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Vehicle所有权在Traffic身上，这里不delete，和ACitizenElement/AVehicleElement同一套
	// 安全原则。
	vehicle = nullptr;
	Super::EndPlay(EndPlayReason);
}

void ATransitVehicleElement::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 只加鼠标视角这一套Context，不加IMC_Default(WASD移动)——搭乘时车自己跑，玩家只能转视角。
	// keyBindings的动态Context由AForeverPlayerController常驻，不在这里管。
	if (APlayerController* playerController = Cast<APlayerController>(NewController)) {
		if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(playerController->GetLocalPlayer())) {
			subsystem->AddMappingContext(inputLookMapping, 0);
		}
		bAboard = true;
		// 在车上就一直显示这辆车的选项(比如停站时的"下车")，不需要触发盒。
		ShowOptions();
	}
}

void ATransitVehicleElement::UnPossessed()
{
	if (APlayerController* playerController = Cast<APlayerController>(GetController())) {
		if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(playerController->GetLocalPlayer())) {
			subsystem->RemoveMappingContext(inputLookMapping);
		}
		bAboard = false;
		bPlayerInBox = false;
		HideOptions();
	}

	Super::UnPossessed();
}

void ATransitVehicleElement::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* enhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		enhancedInput->BindAction(lookAction, ETriggerEvent::Triggered, this, &ATransitVehicleElement::Look);
		enhancedInput->BindAction(mouseLookAction, ETriggerEvent::Triggered, this, &ATransitVehicleElement::Look);

		if (UGameInstance* gameInstance = GetGameInstance()) {
			if (UForeverKeyBindingSubsystem* keyBindings = gameInstance->GetSubsystem<UForeverKeyBindingSubsystem>()) {
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("MeetOptionUp")), ETriggerEvent::Started, this, &ATransitVehicleElement::MeetOptionFocusUp);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("MeetOptionDown")), ETriggerEvent::Started, this, &ATransitVehicleElement::MeetOptionFocusDown);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("MeetOptionSelect")), ETriggerEvent::Started, this, &ATransitVehicleElement::MeetOptionSelect);
			}
		}
	}
}

void ATransitVehicleElement::Look(const FInputActionValue& value)
{
	const FVector2D look = value.Get<FVector2D>();

	if (Controller != nullptr) {
		AddControllerYawInput(look.X);
		AddControllerPitchInput(look.Y);
	}
}

void ATransitVehicleElement::MeetOptionFocusUp()
{
	if (AForeverPlayerController* pc = Cast<AForeverPlayerController>(GetController())) {
		if (UMeetOptionWidget* meetOption = pc->GetMeetOptionWidget()) {
			meetOption->FocusUp();
		}
	}
}

void ATransitVehicleElement::MeetOptionFocusDown()
{
	if (AForeverPlayerController* pc = Cast<AForeverPlayerController>(GetController())) {
		if (UMeetOptionWidget* meetOption = pc->GetMeetOptionWidget()) {
			meetOption->FocusDown();
		}
	}
}

void ATransitVehicleElement::MeetOptionSelect()
{
	if (AForeverPlayerController* pc = Cast<AForeverPlayerController>(GetController())) {
		if (UMeetOptionWidget* meetOption = pc->GetMeetOptionWidget()) {
			meetOption->ClickFocus();
		}
	}
}

void ATransitVehicleElement::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!vehicle) return;

	float x, y, z, yaw;
	vehicle->GetTransform(x, y, z, yaw);
	// Route::Update()直接用地图单位写transform(不像AVehicleElement::Tick那样反过来从UE坐标
	// 换算回去)，这里要乘WORLD_SCALE换成UE单位，和ForeverTrafficFrameworkComponent.cpp里
	// ComputeWorldPosition同一套换算约定。叠加meshYawOffsetDegrees修正模型自己的正前方
	// 和行驶方向没对齐的问题，见Init()里的说明。
	SetActorLocationAndRotation(
		FVector(x * TRANSIT_WORLD_SCALE, y * TRANSIT_WORLD_SCALE, z * TRANSIT_WORLD_SCALE),
		FRotator(0.f, yaw + meshYawOffsetDegrees, 0.f));
}
