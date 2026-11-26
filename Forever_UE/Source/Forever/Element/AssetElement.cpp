#include "Element/AssetElement.h"

#include "Element/FurnitureCollision.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Player/ForeverPlayerController.h"
#include "Player/ForeverCharacter.h"
#include "UI/MeetOptionWidget.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

#include "player/asset.h"

AAssetElement::AAssetElement() {
	mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = mesh;

	// 带物理模拟+重力，玩家走过去能推动它("PhysicsActor"预设+Movable+SimulatePhysics，
	// 角色侧靠UCharacterMovementComponent默认的bEnablePhysicsInteraction施力，无需额外代码)。
	mesh->SetMobility(EComponentMobility::Movable);
	mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	mesh->SetSimulatePhysics(true);
	mesh->SetEnableGravity(true);
	// 球体(投掷物)落地后会一直滚：Chaos里球体的滚动阻力几乎为0，角速度默认不衰减。加角阻尼让
	// 滚动逐渐停下来；不加线性阻尼，免得飞行弧线和预览曲线差得更远。
	mesh->SetAngularDamping(3.f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> cubeFinder(
		TEXT("/Game/Asset/Common/Mesh/Cube.Cube"));
	if (cubeFinder.Succeeded()) {
		mesh->SetStaticMesh(cubeFinder.Object);
	}
}

UStaticMesh* AAssetElement::LoadAssetMesh(const std::string& meshPath) {
	if (meshPath.empty()) return nullptr;

	if (UStaticMesh* loaded = LoadObject<UStaticMesh>(nullptr, UTF8_TO_TCHAR(meshPath.c_str()))) {
		return loaded;
	}
	UE_LOG(LogTemp, Warning, TEXT("资产网格加载失败: %s，回退引擎自带球体"), UTF8_TO_TCHAR(meshPath.c_str()));
	return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
}

void AAssetElement::Init(Asset* inAsset) {
	asset = inAsset;
	if (!asset) return;

	// 缩放不在这里设：本Actor的根就是这个网格组件，缩放由生成方通过SpawnActorDeferred的
	// Transform传入(物理体在BeginPlay时就按这个缩放创建)，见UForeverAssetFrameworkComponent::
	// SpawnWorldAsset。
	if (UStaticMesh* customMesh = LoadAssetMesh(asset->GetMeshPath())) {
		mesh->SetStaticMesh(customMesh);
	}

	if (asset->IsFixture()) SetupFixture();
	BuildProximityBox();
}

void AAssetElement::SetupFixture() {
	// 固定物品不动：不模拟物理、不受重力，网格只负责显示(家具网格没有自带的碰撞体)。
	mesh->SetSimulatePhysics(false);
	mesh->SetEnableGravity(false);
	mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	mesh->SetGenerateOverlapEvents(false);
	mesh->SetCastShadow(false); // 和房间家具一致：室内小件不投影
	mesh->SetMobility(EComponentMobility::Static);

	UStaticMesh* staticMesh = mesh->GetStaticMesh();
	if (!staticMesh) return;
	// 阻挡：和房间家具同一套分块盒子(网格局部系)。盒子挂在根下面，根的缩放自动作用到盒子的位置和尺寸上。
	const FBoxSphereBounds bounds = staticMesh->GetBounds();
	const FVector boundsMin = bounds.Origin - bounds.BoxExtent, boundsMax = bounds.Origin + bounds.BoxExtent;
	if ((boundsMax.Z - boundsMin.Z) * GetActorScale3D().Z < FurnitureCollision::kMinBlockingHeight) return; // 薄片不挡路
	TArray<FBox> boxes;
	FurnitureCollision::GetBoxes(UTF8_TO_TCHAR(asset->GetMeshPath().c_str()), boundsMin, boundsMax, boxes);
	for (const FBox& part : boxes) {
		UBoxComponent* blocker = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
		blocker->SetMobility(EComponentMobility::Static);
		blocker->SetBoxExtent(part.GetExtent());
		blocker->SetupAttachment(RootComponent);
		blocker->SetRelativeLocation(part.GetCenter());
		blocker->SetGenerateOverlapEvents(false);
		blocker->SetCanEverAffectNavigation(false);
		FurnitureCollision::ApplyBlockingResponse(blocker);
		blocker->RegisterComponent();
	}
}

void AAssetElement::BuildProximityBox() {
	if (!asset || !asset->GetScript() || proximityBox) return;

	// 根组件带缩放(meshScale)，子组件的局部尺寸要除掉它，才是世界里120x120x80cm的一圈。
	FVector scale = GetActorScale3D();
	scale.X = FMath::Max(scale.X, 0.01f);
	scale.Y = FMath::Max(scale.Y, 0.01f);
	scale.Z = FMath::Max(scale.Z, 0.01f);

	UBoxComponent* box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
	UStaticMesh* staticMesh = mesh->GetStaticMesh();
	if (asset->IsFixture() && staticMesh) {
		// 固定物品(床之类)个头大：触发盒取网格包围盒，世界里水平方向各外扩80cm、上下各外扩20cm。
		const FBoxSphereBounds bounds = staticMesh->GetBounds();
		box->SetBoxExtent(bounds.BoxExtent + FVector(80.f, 80.f, 20.f) / scale);
		box->SetRelativeLocation(bounds.Origin);
		box->SetMobility(EComponentMobility::Static);
	}
	else {
		box->SetBoxExtent(FVector(120.f, 120.f, 80.f) / scale);
	}
	box->SetCollisionProfileName(TEXT("Trigger"));
	box->SetupAttachment(RootComponent);
	box->OnComponentBeginOverlap.AddDynamic(this, &AAssetElement::OnOverlapBegin);
	box->OnComponentEndOverlap.AddDynamic(this, &AAssetElement::OnOverlapEnd);
	box->RegisterComponent();
	proximityBox = box;

	assetNameOnly = UTF8_TO_TCHAR(asset->GetName().c_str());
	BakeOptions();
}

void AAssetElement::BakeOptions() {
	if (!asset) return;
	cachedOptions.Reset();
	for (const std::string& option : asset->GetOptions()) {
		cachedOptions.Add(UTF8_TO_TCHAR(option.c_str()));
	}
}

void AAssetElement::ShowOptions() {
	AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	UMeetOptionWidget* meetOption = playerController ? playerController->GetMeetOptionWidget() : nullptr;
	if (!meetOption) return;

	AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	// 先清掉这件资产名下的旧按钮再逐个加，不依赖ReplaceOptions(名下没有按钮时它是空操作)。
	meetOption->RemoveName(assetNameOnly);
	for (int32 idx = 0; idx < cachedOptions.Num(); idx++) {
		meetOption->AddOption(frameworkActor, cachedOptions[idx], assetNameOnly, idx);
	}
}

void AAssetElement::HideOptions() {
	if (AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))) {
		if (UMeetOptionWidget* meetOption = playerController->GetMeetOptionWidget()) {
			meetOption->RemoveName(assetNameOnly);
		}
	}
}

void AAssetElement::RefreshOptions() {
	if (!proximityBox || !asset) return;
	BakeOptions();
	if (bPlayerInBox) ShowOptions();
}

void AAssetElement::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	// 只有玩家当前控制的AForeverCharacter(人物)走近才弹选项，开车经过不算。
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn || !Cast<AForeverCharacter>(OtherActor)) return;
	bPlayerInBox = true;
	ShowOptions();
}

void AAssetElement::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn || !Cast<AForeverCharacter>(OtherActor)) return;
	bPlayerInBox = false;
	HideOptions();
}

void AAssetElement::BeginPlay() {
	Super::BeginPlay();
	// 流式生成时玩家可能已经站在触发盒里：主动刷新一次重叠，补发BeginOverlap(否则要走出去再走回来才弹选项)。
	if (proximityBox) proximityBox->UpdateOverlaps();
}

void AAssetElement::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	// 被捡走/销毁时玩家可能还站在盒里，引擎不一定补发End：主动撤掉屏幕上这件资产的选项。
	if (bPlayerInBox) HideOptions();
	bPlayerInBox = false;
	asset = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AAssetElement::Launch(const FVector& velocity, AActor* thrower) {
	if (thrower) mesh->IgnoreActorWhenMoving(thrower, true);
	mesh->SetUseCCD(true);
	mesh->SetPhysicsLinearVelocity(velocity);
}
