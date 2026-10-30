#include "Element/AssetElement.h"

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

	// 带物理模拟+重力，玩家角色走过去能把它推动——不是摆设用的Static网格，见用户明确要求。
	// "PhysicsActor"是引擎内置碰撞预设(ObjectType=PhysicsBody，对WorldStatic/WorldDynamic/
	// Pawn/PhysicsBody等都是Block)，配合Movable+SimulatePhysics就是标准的"可推动物体"做法，
	// 角色这边不需要加任何代码——UCharacterMovementComponent默认bEnablePhysicsInteraction=
	// true，走进去自然会对撞到的物理物体施加推力。
	mesh->SetMobility(EComponentMobility::Movable);
	mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	mesh->SetSimulatePhysics(true);
	mesh->SetEnableGravity(true);
	// 球体(投掷物)落地后会一直滚：Chaos里球体的滚动阻力几乎为0，角速度默认不衰减。加角阻尼让
	// 滚动逐渐停下来；不加线性阻尼，免得飞行弧线和预览曲线差得更远。
	mesh->SetAngularDamping(3.f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> cubeFinder(
		TEXT("/Game/Asset/Meshes/Cube.Cube"));
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

	BuildProximityBox();
}

void AAssetElement::BuildProximityBox() {
	if (!asset || !asset->GetScript() || proximityBox) return;

	// 根组件带缩放(meshScale)，子组件的局部尺寸要除掉它，才是世界里120x120x80cm的一圈。
	FVector scale = GetActorScale3D();
	scale.X = FMath::Max(scale.X, 0.01f);
	scale.Y = FMath::Max(scale.Y, 0.01f);
	scale.Z = FMath::Max(scale.Z, 0.01f);

	UBoxComponent* box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
	box->SetBoxExtent(FVector(120.f, 120.f, 80.f) / scale);
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
