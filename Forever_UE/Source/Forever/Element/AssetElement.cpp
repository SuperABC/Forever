#include "Element/AssetElement.h"

#include "Components/StaticMeshComponent.h"
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
}

void AAssetElement::Launch(const FVector& velocity, AActor* thrower) {
	if (thrower) mesh->IgnoreActorWhenMoving(thrower, true);
	mesh->SetUseCCD(true);
	mesh->SetPhysicsLinearVelocity(velocity);
}
