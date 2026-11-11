#include "Element/DoorComponent.h"

#include "map/door.h"
#include "populace/citizen.h"

#include "Element/CitizenElement.h"
#include "Element/DoorAnimInstance.h"
#include "Element/VehicleElement.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Player/ForeverPlayerController.h"
#include "UI/MeetOptionWidget.h"

#include "Animation/AnimInstance.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/GameplayStatics.h"

#define DOOR_WORLD_SCALE 1000.f // 地图单位->UE单位，和Building/ZoneWorldScale同一个数值
#define DOOR_DEFAULT_OPEN_SECONDS 0.5f
#define DOOR_CLOSE_DELAY_SECONDS 1.f // 触发盒空了之后延迟这么久才真正关门
// 挡板厚度(UE单位，即厘米)：和墙一样厚(BuildingElement.cpp::BUILDING_WALL_THICKNESS=0.01地图单位=10UE单位)，
// 关着的门把整个门洞堵死。骨骼网格自己的厚度来自资产(按门宽等比)，和这个无关。
#define DOOR_BLOCKER_THICKNESS 10.f
#define DOOR_BLOCKER_OPEN_THRESHOLD 0.5f // 开合度到这个值以上挡板放行

namespace {
	// 门资产缓存：按软路径缓存已解析的骨骼网格/动画蓝图类，避免每扇门Init()各自Load(近LOD生成会变卡)；
	// 用TWeakObjectPtr避免永久持有资产不被GC。
	TMap<FString, TWeakObjectPtr<USkeletalMesh>> GDoorMeshCache;
	TMap<FString, TWeakObjectPtr<UClass>> GDoorAnimClassCache;

	USkeletalMesh* LoadDoorMesh(const FString& path) {
		if (path.IsEmpty()) return nullptr;
		if (TWeakObjectPtr<USkeletalMesh>* cached = GDoorMeshCache.Find(path)) {
			if (USkeletalMesh* mesh = cached->Get()) return mesh;
		}
		USkeletalMesh* mesh = LoadObject<USkeletalMesh>(nullptr, *path);
		GDoorMeshCache.Add(path, mesh);
		return mesh;
	}

	// 动画蓝图类，加载失败或没配路径回退到UDoorAnimInstance基类(门不动)。
	UClass* LoadDoorAnimClass(const FString& path) {
		if (!path.IsEmpty()) {
			if (TWeakObjectPtr<UClass>* cached = GDoorAnimClassCache.Find(path)) {
				if (UClass* animClass = cached->Get()) return animClass;
			}
			UClass* animClass = LoadClass<UAnimInstance>(nullptr, *path);
			if (animClass) {
				GDoorAnimClassCache.Add(path, animClass);
				return animClass;
			}
			UE_LOG(LogTemp, Warning, TEXT("UForeverDoorComponent: 门动画蓝图加载失败，回退到UDoorAnimInstance: %s"), *path);
		}
		return UDoorAnimInstance::StaticClass();
	}
}

UForeverDoorComponent::UForeverDoorComponent() {
	PrimaryComponentTick.bCanEverTick = true;
}

TMap<const Door*, TWeakObjectPtr<UForeverDoorComponent>> UForeverDoorComponent::componentsByDoor;

UForeverDoorComponent* UForeverDoorComponent::FindForDoor(const Door* door) {
	const TWeakObjectPtr<UForeverDoorComponent>* found = componentsByDoor.Find(door);
	return found ? found->Get() : nullptr;
}

void UForeverDoorComponent::OnComponentDestroyed(bool bDestroyingHierarchy) {
	if (door) {
		const TWeakObjectPtr<UForeverDoorComponent>* found = componentsByDoor.Find(door);
		if (found && found->Get() == this) componentsByDoor.Remove(door);
	}
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UForeverDoorComponent::Init(Door* inDoor) {
	door = inDoor;
	if (!door) return;
	componentsByDoor.Add(door, this);

	// Door::GetYaw()是墙体外法线方向(弧度)——这个组件自己的local X要对齐"沿墙"方向(资产
	// 约定"X轴=沿墙方向")，和法线正好相差90度，见door.md"坐标约定"一节。
	float componentYawDegrees = FMath::RadiansToDegrees(door->GetYaw()) - 90.f;
	SetWorldLocation(FVector(door->GetX() * DOOR_WORLD_SCALE, door->GetY() * DOOR_WORLD_SCALE,
		door->GetZ() * DOOR_WORLD_SCALE));
	SetWorldRotation(FRotator(0.f, componentYawDegrees, 0.f));

	float doorWidth = door->GetWidth() * DOOR_WORLD_SCALE;
	float doorHeight = door->GetHeight() * DOOR_WORLD_SCALE;

	USkeletalMesh* mesh = LoadDoorMesh(UTF8_TO_TCHAR(door->GetMesh().c_str()));
	if (!mesh) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverDoorComponent::Init: 门资产加载失败: %s (交互名=%s)"),
			UTF8_TO_TCHAR(door->GetMesh().c_str()), UTF8_TO_TCHAR(door->GetInteractName().c_str()));
		return;
	}

	BuildSkeletalDoor(mesh, doorWidth, doorHeight);
	BuildBlocker(doorWidth, doorHeight);
	BuildTriggerBox(doorWidth, doorHeight);

	interactNameOnly = UTF8_TO_TCHAR(door->GetInteractName().c_str());
	cachedOptions.Reset();
	for (const std::string& option : door->GetOptions()) {
		cachedOptions.Add(UTF8_TO_TCHAR(option.c_str()));
	}
}

void UForeverDoorComponent::BuildSkeletalDoor(USkeletalMesh* mesh, float doorWidth, float doorHeight) {
	USkeletalMeshComponent* comp = NewObject<USkeletalMeshComponent>(this, NAME_None, RF_Transient);
	comp->SetSkeletalMeshAsset(mesh);
	if (mesh->GetPhysicsAsset()) {
		// 骨骼网格带Physics Asset：门扇的碰撞盒挂在door骨上，kinematic随动画转动，开着的门扇也挡人。
		// 只做查询碰撞(挡角色/载具)，不模拟物理，也不产生重叠事件(否则会和自己的触发盒互相触发)。
		comp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		comp->SetGenerateOverlapEvents(false);
		comp->SetCanEverAffectNavigation(false);
	} else {
		comp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 没有Physics Asset：只靠BuildBlocker的挡板挡关着的门
	}
	comp->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	comp->SetAnimInstanceClass(LoadDoorAnimClass(UTF8_TO_TCHAR(door->GetAnimClass().c_str())));
	// 不按可见性跳过姿势评估：门全关后会暂停动画，若暂停前一次都没评估过(刚生成时还没被渲染)，
	// 门就会停在动画蓝图未求值的初始姿势上(表现为初始开着，靠近后才瞬间关上)。
	comp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	comp->SetupAttachment(this);
	comp->RegisterComponent();

	// 立刻按开合度0同步评估一次姿势，保证第一帧就是关着的。
	comp->InitAnim(true);
	if (UDoorAnimInstance* anim = Cast<UDoorAnimInstance>(comp->GetAnimInstance())) {
		anim->OpenProgress = 0.f;
		anim->OpenSide = openSide;
	}
	comp->TickAnimation(0.f, false);
	comp->RefreshBoneTransforms();

	// 对齐门洞：X、Y等比(系数=门宽/包围盒宽)，Z独立到门高；包围盒中心落在(0,0,门高/2)，不管资产pivot在哪。
	FBoxSphereBounds bounds = mesh->GetBounds();
	FVector extent = bounds.BoxExtent;
	float scaleXY = extent.X > KINDA_SMALL_NUMBER ? doorWidth / (2.f * extent.X) : 1.f;
	float scaleZ = extent.Z > KINDA_SMALL_NUMBER ? doorHeight / (2.f * extent.Z) : 1.f;
	FVector scale(scaleXY, scaleXY, scaleZ);
	comp->SetRelativeScale3D(scale);
	comp->SetRelativeLocation(FVector(0.f, 0.f, doorHeight * 0.5f) -
		FVector(scale.X * bounds.Origin.X, scale.Y * bounds.Origin.Y, scale.Z * bounds.Origin.Z));

	doorMesh = comp;
}

void UForeverDoorComponent::BuildBlocker(float doorWidth, float doorHeight) {
	UBoxComponent* box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
	box->SetBoxExtent(FVector(doorWidth * 0.5f, DOOR_BLOCKER_THICKNESS * 0.5f, doorHeight * 0.5f));
	box->SetRelativeLocation(FVector(0.f, 0.f, doorHeight * 0.5f));
	box->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	box->SetGenerateOverlapEvents(false);
	box->SetCanEverAffectNavigation(false);
	box->SetHiddenInGame(true);
	box->SetMobility(EComponentMobility::Movable);
	box->SetupAttachment(this);
	box->RegisterComponent();

	blocker = box;
	bBlockerEnabled = true;
}

void UForeverDoorComponent::BuildTriggerBox(float doorWidth, float doorHeight) {
	UBoxComponent* box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
	// 门两侧各伸出1.5米；车行门伸出4米。注意单位：DOOR_WORLD_SCALE是地图单位->UE单位(1地图单位=10米)，
	// 这里的米数必须先换算成地图单位(米数/10)再乘DOOR_WORLD_SCALE，否则触发范围会放大10倍。
	float paddingMeters = door->IsVehicleGate() ? 4.f : 1.5f;
	float padding = (paddingMeters / 10.f) * DOOR_WORLD_SCALE;
	box->SetBoxExtent(FVector(doorWidth * 0.5f + padding * (door->IsVehicleGate() ? 1.5f : 1.f),
		padding, doorHeight * 0.5f));
	box->SetRelativeLocation(FVector(0.f, 0.f, doorHeight * 0.5f));
	box->SetCollisionProfileName(TEXT("Trigger"));
	box->SetMobility(EComponentMobility::Movable);
	box->SetupAttachment(this);
	box->OnComponentBeginOverlap.AddDynamic(this, &UForeverDoorComponent::OnOverlapBegin);
	box->OnComponentEndOverlap.AddDynamic(this, &UForeverDoorComponent::OnOverlapEnd);
	box->RegisterComponent();

	trigger = box;
}

void UForeverDoorComponent::ResolvePasserIdentity(AActor* actor, Citizen*& outCitizen, bool& outIsPlayer) const {
	outCitizen = nullptr;
	outIsPlayer = false;

	APawn* playerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (actor == playerPawn) {
		outIsPlayer = true;
		return;
	}
	if (ACitizenElement* citizenElement = Cast<ACitizenElement>(actor)) {
		outCitizen = citizenElement->GetCitizen();
		return;
	}
	if (AVehicleElement* vehicle = Cast<AVehicleElement>(actor)) {
		if (AActor* driver = vehicle->GetPreviousPawn().Get()) {
			// 空车(没人在开)GetPreviousPawn()返回这辆车被真正驾驶前"玩家原来的pawn"，这个
			// 字段只在车被真正驾驶期间有意义——用IsPlayerControlled()排除掉"车辆本身一直
			// 停在原地、从没被开过"的情况：没被开过的车不应该被当成"有驾驶员"。
			if (vehicle->IsPlayerControlled()) {
				outIsPlayer = true;
			} else if (ACitizenElement* driverCitizen = Cast<ACitizenElement>(driver)) {
				outCitizen = driverCitizen->GetCitizen();
			}
		}
		return;
	}
	// 其余(公交载具等)：身份判定不出来，按(nullptr,false)处理，只有Open的门会给它开，见
	// Door::CanPass。
}

void UForeverDoorComponent::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	if (!door || !OtherActor || OtherActor == GetOwner()) return;

	Citizen* citizen = nullptr;
	bool isPlayer = false;
	ResolvePasserIdentity(OtherActor, citizen, isPlayer);

	if (door->CanPass(citizen, isPlayer)) {
		// 门是从全关状态被这个人推开的——按他在哪一侧决定往另一侧开。门已经开着/没关严时
		// 不改方向，避免门在半空中反向翻转(后来的人从另一侧过来就顺着已经开的方向走)。
		if (!bWantsOpen && openProgress <= KINDA_SMALL_NUMBER) ChooseOpenSide(OtherActor);
		passers.Add(OtherActor);
		UpdateOpenState();
	}

	// 交互选项(MeetOption)只认玩家本人靠近，门禁拒绝通行也照常弹出——拒绝通行和拒绝对话是
	// 两件独立的事，见door.md。
	APawn* playerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (OtherActor != playerPawn || door->GetScript() == nullptr || cachedOptions.Num() == 0) return;

	if (AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))) {
		if (UMeetOptionWidget* meetOption = playerController->GetMeetOptionWidget()) {
			AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(
				UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
			for (int32 idx = 0; idx < cachedOptions.Num(); idx++) {
				meetOption->AddOption(frameworkActor, cachedOptions[idx], interactNameOnly, idx);
			}
		}
	}
}

void UForeverDoorComponent::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	if (!OtherActor) return;

	passers.Remove(OtherActor);
	UpdateOpenState();

	APawn* playerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (OtherActor != playerPawn || !door || door->GetScript() == nullptr) return;

	if (AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))) {
		if (UMeetOptionWidget* meetOption = playerController->GetMeetOptionWidget()) {
			meetOption->RemoveName(interactNameOnly);
		}
	}
}

void UForeverDoorComponent::ChooseOpenSide(AActor* approacher) {
	if (!approacher) return;
	// 组件本地+Y就是墙法线方向(local X=沿墙，见Init里componentYaw=法线yaw-90)。触发者在
	// +Y一侧就往-Y开，反之往+Y开——永远往远离触发者的一侧推开。
	float approacherLocalY = GetComponentTransform().InverseTransformPosition(approacher->GetActorLocation()).Y;
	openSide = approacherLocalY >= 0.f ? -1.f : 1.f;
}

void UForeverDoorComponent::UpdateOpenState() {
	// passers先清理掉已经失效的弱指针，避免悬空Actor一直占着"该开门"的状态。
	for (auto it = passers.CreateIterator(); it; ++it) {
		if (!it->IsValid()) it.RemoveCurrent();
	}

	if (passers.Num() > 0) {
		bWantsOpen = true;
		closeAtTime = -1.f;
	} else if (bWantsOpen) {
		closeAtTime = GetWorld() ? GetWorld()->GetTimeSeconds() + DOOR_CLOSE_DELAY_SECONDS : -1.f;
	}

	// 唤醒：恢复动画评估，重新计空闲帧。
	bAnimPaused = false;
	idleTicks = 0;
	if (doorMesh) doorMesh->bPauseAnims = false;
	SetComponentTickEnabled(true);
}

void UForeverDoorComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (closeAtTime >= 0.f && GetWorld() && GetWorld()->GetTimeSeconds() >= closeAtTime) {
		bWantsOpen = false;
		closeAtTime = -1.f;
	}

	float target = bWantsOpen ? 1.f : 0.f;
	float openSeconds = door && door->GetOpenSeconds() > 0.f ? door->GetOpenSeconds() : DOOR_DEFAULT_OPEN_SECONDS;
	float speed = openSeconds > 0.f ? 1.f / openSeconds : 1.f;
	openProgress = FMath::FInterpConstantTo(openProgress, target, DeltaTime, speed);

	// 把开合度写给动画实例，模型怎么动由动画蓝图决定(缓动也在动画蓝图里加)。
	if (doorMesh) {
		if (UDoorAnimInstance* anim = Cast<UDoorAnimInstance>(doorMesh->GetAnimInstance())) {
			anim->OpenProgress = openProgress;
			anim->OpenSide = openSide;
		} else if (!bWarnedAnimClass) {
			bWarnedAnimClass = true;
			UE_LOG(LogTemp, Warning, TEXT("UForeverDoorComponent: 门的动画实例不是UDoorAnimInstance(动画蓝图父类要选DoorAnimInstance)，门不会动: %s"),
				door ? UTF8_TO_TCHAR(door->GetAnimClass().c_str()) : TEXT(""));
		}
	}

	// 挡板：关着挡人，开到阈值以上放行；只在状态变化时切换。
	if (blocker) {
		bool shouldBlock = openProgress < DOOR_BLOCKER_OPEN_THRESHOLD;
		if (shouldBlock != bBlockerEnabled) {
			bBlockerEnabled = shouldBlock;
			blocker->SetCollisionEnabled(shouldBlock ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		}
	}

	// 全关且没有待处理的开门请求：多Tick一帧让动画实例评估一次0，之后暂停动画并停Tick，见door.md"性能"一节。
	if (FMath::IsNearlyEqual(openProgress, target) && FMath::IsNearlyEqual(openProgress, 0.f, 0.001f)) {
		if (++idleTicks >= 2) {
			if (doorMesh) doorMesh->bPauseAnims = true;
			bAnimPaused = true;
			SetComponentTickEnabled(false);
		}
	} else {
		idleTicks = 0;
	}
}

void UForeverDoorComponent::RefreshOptions() {
	if (!door) return;
	cachedOptions.Reset();
	for (const std::string& option : door->GetOptions()) {
		cachedOptions.Add(UTF8_TO_TCHAR(option.c_str()));
	}
	if (AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))) {
		if (UMeetOptionWidget* meetOption = playerController->GetMeetOptionWidget()) {
			AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(
				UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
			meetOption->ReplaceOptions(frameworkActor, interactNameOnly, cachedOptions);
		}
	}
}
