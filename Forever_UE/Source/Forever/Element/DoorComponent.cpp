#include "Element/DoorComponent.h"

#include "map/door.h"
#include "populace/citizen.h"

#include "Element/CitizenElement.h"
#include "Element/VehicleElement.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Player/ForeverPlayerController.h"
#include "UI/MeetOptionWidget.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"

#define DOOR_WORLD_SCALE 1000.f // 地图单位->UE单位，和Building/ZoneWorldScale同一个数值
#define DOOR_DEFAULT_OPEN_SECONDS 0.5f
#define DOOR_CLOSE_DELAY_SECONDS 1.f // 触发盒空了之后延迟这么久才真正关门
// 门扇/门框的厚度(UE单位，即厘米)——必须比墙薄，否则门扇会凸出墙面两侧。墙厚是
// BuildingElement.cpp::BUILDING_WALL_THICKNESS(0.01地图单位=10UE单位=10厘米)，这里固定
// 6厘米，不用资产自己的原始厚度(最初直接传0.f让AlignMeshToBounds保留Cube资产原厚度，
// 结果比墙还厚，实测复现)。
#define DOOR_THICKNESS 6.f

namespace {
	// 门资产缓存：按软路径缓存已解析的UStaticMesh*，同一个路径(目前所有门都用占位Cube)不用
	// 每扇门各自重新LoadObject一次——加门之后近LOD生成变卡，排查发现每扇门Init()都各自独立
	// LoadObject，没有走ABuildingElement::SpawnCube/SpawnMesh那套"按软路径缓存"的惯例
	// (ForeverBuildingFrameworkComponent.md里meshCache/lodMaterialCache同款做法)，这里补上
	// 同样的缓存。用TWeakObjectPtr而不是硬引用，避免永久持有资产不被GC。
	TMap<FString, TWeakObjectPtr<UStaticMesh>> GDoorMeshCache;

	UStaticMesh* LoadDoorMesh(const FString& path) {
		if (path.IsEmpty()) return nullptr;
		if (TWeakObjectPtr<UStaticMesh>* cached = GDoorMeshCache.Find(path)) {
			if (UStaticMesh* mesh = cached->Get()) return mesh;
		}
		UStaticMesh* mesh = LoadObject<UStaticMesh>(nullptr, *path);
		GDoorMeshCache.Add(path, mesh);
		return mesh;
	}
}

UForeverDoorComponent::UForeverDoorComponent() {
	PrimaryComponentTick.bCanEverTick = true;
}

void UForeverDoorComponent::AlignMeshToBounds(UStaticMeshComponent* meshComp, UStaticMesh* mesh,
	const FVector& desiredLocalSize, const FVector& desiredLocalCenterOffset) const {
	if (!meshComp || !mesh) return;

	FBoxSphereBounds bounds = mesh->GetBounds();
	FVector extent = bounds.BoxExtent;
	// desiredLocalSize某个轴给0.f表示"这个轴不缩放，保留资产原样"(比如厚度轴，见
	// BuildLeaves/BuildFrame的调用)——必须同时判desiredLocalSize>0，否则0/(2*extent)算出
	// 缩放系数0，把这个轴整个压扁成0厚度，门扇/门框整体不可见(实测复现：所有门在场景里
	// 完全看不见，见DoorComponent.md"已知简化"一节)。
	FVector scale(
		(extent.X > KINDA_SMALL_NUMBER && desiredLocalSize.X > KINDA_SMALL_NUMBER) ? desiredLocalSize.X / (2.f * extent.X) : 1.f,
		(extent.Y > KINDA_SMALL_NUMBER && desiredLocalSize.Y > KINDA_SMALL_NUMBER) ? desiredLocalSize.Y / (2.f * extent.Y) : 1.f,
		(extent.Z > KINDA_SMALL_NUMBER && desiredLocalSize.Z > KINDA_SMALL_NUMBER) ? desiredLocalSize.Z / (2.f * extent.Z) : 1.f);
	meshComp->SetRelativeScale3D(scale);

	FVector origin = bounds.Origin;
	FVector relLoc = desiredLocalCenterOffset -
		FVector(scale.X * origin.X, scale.Y * origin.Y, scale.Z * origin.Z);
	meshComp->SetRelativeLocation(relLoc);
}

void UForeverDoorComponent::Init(Door* inDoor) {
	door = inDoor;
	if (!door) return;

	// Door::GetYaw()是墙体外法线方向(弧度)——这个组件自己的local X要对齐"沿墙"方向(资产
	// 约定"X轴=沿墙方向")，和法线正好相差90度，见door.md"坐标约定"一节。
	float componentYawDegrees = FMath::RadiansToDegrees(door->GetYaw()) - 90.f;
	SetWorldLocation(FVector(door->GetX() * DOOR_WORLD_SCALE, door->GetY() * DOOR_WORLD_SCALE,
		door->GetZ() * DOOR_WORLD_SCALE));
	SetWorldRotation(FRotator(0.f, componentYawDegrees, 0.f));

	float doorWidth = door->GetWidth() * DOOR_WORLD_SCALE;
	float doorHeight = door->GetHeight() * DOOR_WORLD_SCALE;

	UStaticMesh* mesh = LoadDoorMesh(UTF8_TO_TCHAR(door->GetMesh().c_str()));
	if (!mesh) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverDoorComponent::Init: 门资产加载失败: %s (交互名=%s)"),
			UTF8_TO_TCHAR(door->GetMesh().c_str()), UTF8_TO_TCHAR(door->GetInteractName().c_str()));
		return;
	}

	BuildLeaves(mesh, doorWidth, doorHeight, DOOR_THICKNESS);

	if (!door->GetFrameMesh().empty()) {
		UStaticMesh* frameMesh = LoadDoorMesh(UTF8_TO_TCHAR(door->GetFrameMesh().c_str()));
		if (frameMesh) BuildFrame(frameMesh, doorWidth, doorHeight, DOOR_THICKNESS);
	}

	BuildTriggerBox(doorWidth, doorHeight);

	interactNameOnly = UTF8_TO_TCHAR(door->GetInteractName().c_str());
	cachedOptions.Reset();
	for (const std::string& option : door->GetOptions()) {
		cachedOptions.Add(UTF8_TO_TCHAR(option.c_str()));
	}
}

void UForeverDoorComponent::BuildLeaves(UStaticMesh* mesh, float doorWidth, float doorHeight, float thickness) {
	int32 leafCount = door->GetLeaves() == 2 ? 2 : 1;
	bool isSwing = (door->GetStyle() == 1);
	float amount = door->GetOpenAmount() > 0.f ? door->GetOpenAmount() : (isSwing ? 90.f : 1.f);

	for (int32 leafIndex = 0; leafIndex < leafCount; leafIndex++) {
		USceneComponent* pivot = NewObject<USceneComponent>(this, NAME_None, RF_Transient);
		pivot->SetupAttachment(this);
		pivot->RegisterComponent();

		UStaticMeshComponent* meshComp = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		meshComp->SetStaticMesh(mesh);
		meshComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		meshComp->SetMobility(EComponentMobility::Movable);
		meshComp->SetGenerateOverlapEvents(false);
		meshComp->SetupAttachment(pivot);
		meshComp->RegisterComponent();

		FTransform closed, open;

		if (!isSwing) {
			// Slide：leaves==1整扇覆盖[-doorWidth/2, doorWidth/2]，居中(pivot本地X=0)；
			// leaves==2每扇各自覆盖一半，pivot本地X偏移到各自那一半的中心。
			float leafWidth = leafCount == 2 ? doorWidth * 0.5f : doorWidth;
			float restX = leafCount == 2 ? (leafIndex == 0 ? -leafWidth * 0.5f : leafWidth * 0.5f) : 0.f;
			AlignMeshToBounds(meshComp, mesh, FVector(leafWidth, thickness, doorHeight),
				FVector(0.f, 0.f, doorHeight * 0.5f));

			float slideDist = leafWidth * amount;
			float dir = leafCount == 2 ? (leafIndex == 0 ? -1.f : 1.f) : (door->IsFlippedSide() ? -1.f : 1.f);

			closed = FTransform(FRotator::ZeroRotator, FVector(restX, 0.f, 0.f));
			open = FTransform(FRotator::ZeroRotator, FVector(restX + dir * slideDist, 0.f, 0.f));
		} else {
			// Swing：leaves==1铰链在洞口某一侧边缘(IsFlippedSide()决定哪一侧)，门扇从铰链
			// 延伸到洞口另一端(整个doorWidth)；leaves==2两扇各自铰链在洞口两端外侧边缘，
			// 各覆盖半个doorWidth，对称打开。门扇mesh的包围盒中心要落在"离铰链半个扇宽"处
			// (pivot本地+X方向，因为mesh局部X轴就是"沿墙"方向)，开门时pivot绕本地Z轴转
			// amount度——这次固定往本地+Y方向转(简化掉"动态判断朝哪边开避免扫到人"这个
			// 细节，见door.md"已知简化"一节)，不按door这一次具体是谁触发来决定方向。
			float leafWidth = leafCount == 2 ? doorWidth * 0.5f : doorWidth;
			float hingeX;
			if (leafCount == 2) {
				hingeX = (leafIndex == 0) ? -doorWidth * 0.5f : doorWidth * 0.5f;
			} else {
				hingeX = door->IsFlippedSide() ? doorWidth * 0.5f : -doorWidth * 0.5f;
			}
			// 门扇从铰链"朝洞口内侧"延伸：leafIndex0(或唯一扇朝+X铰链在-X时)延伸方向取决于
			// 铰链在哪一侧——铰链在左(-X)就朝+X延伸，铰链在右(+X)就朝-X延伸；双扇时两扇都
			// 朝洞口中心延伸。
			float extendSign = (hingeX < 0.f) ? 1.f : -1.f;
			AlignMeshToBounds(meshComp, mesh, FVector(leafWidth, thickness, doorHeight),
				FVector(extendSign * leafWidth * 0.5f, 0.f, doorHeight * 0.5f));

			float openSign = (leafCount == 2 && leafIndex == 1) ? -1.f : 1.f;
			closed = FTransform(FRotator::ZeroRotator, FVector(hingeX, 0.f, 0.f));
			open = FTransform(FRotator(0.f, openSign * amount, 0.f), FVector(hingeX, 0.f, 0.f));
		}

		pivot->SetRelativeTransform(closed);

		FLeaf leaf;
		leaf.pivot = pivot;
		leaf.mesh = meshComp;
		leaf.closedTransform = closed;
		leaf.openTransform = open;
		leaves.Add(leaf);
	}
}

void UForeverDoorComponent::BuildFrame(UStaticMesh* frameMesh, float doorWidth, float doorHeight, float thickness) {
	UStaticMeshComponent* comp = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	comp->SetStaticMesh(frameMesh);
	comp->SetCollisionProfileName(TEXT("BlockAll"));
	comp->SetMobility(EComponentMobility::Static);
	comp->SetGenerateOverlapEvents(false);
	comp->SetupAttachment(this);
	comp->RegisterComponent();

	AlignMeshToBounds(comp, frameMesh, FVector(doorWidth, thickness, doorHeight),
		FVector(0.f, 0.f, doorHeight * 0.5f));

	frameMeshComp = comp;
}

void UForeverDoorComponent::BuildTriggerBox(float doorWidth, float doorHeight) {
	UBoxComponent* box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
	// 门两侧各伸出1.5米；车行门伸出4米，宽度也相应放大，见door_system_plan.md"UE：自动门
	// 组件"一节。注意单位：DOOR_WORLD_SCALE是"地图单位->UE单位"的换算系数(1地图单位=
	// 1000UE单位=10米)，这里要的是"米"，不是"地图单位"——最初直接写成1.5/4.0地图单位，
	// 换算出15米/40米的触发范围，导致离门老远就自动开门(实测复现)，这里改成先换算成地图
	// 单位(米数/10)再乘DOOR_WORLD_SCALE。
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
				meetOption->AddOption(frameworkActor, cachedOptions[idx], interactNameOnly, idx, false);
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
	float t = FMath::SmoothStep(0.f, 1.f, openProgress);

	for (FLeaf& leaf : leaves) {
		if (!leaf.pivot) continue;
		FVector loc = FMath::Lerp(leaf.closedTransform.GetLocation(), leaf.openTransform.GetLocation(), t);
		FQuat rot = FQuat::Slerp(leaf.closedTransform.GetRotation(), leaf.openTransform.GetRotation(), t);
		leaf.pivot->SetRelativeTransform(FTransform(rot, loc), false, nullptr, ETeleportType::None);
	}

	if (FMath::IsNearlyEqual(openProgress, target) && FMath::IsNearlyEqual(openProgress, 0.f, 0.001f)) {
		SetComponentTickEnabled(false); // 全关且没有待处理的开门请求时停Tick，见door.md"性能"一节
	}
}
