#include "Element/AssetElement.h"

#include "Components/StaticMeshComponent.h"
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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> cubeFinder(
		TEXT("/Game/Asset/Meshes/Cube.Cube"));
	if (cubeFinder.Succeeded()) {
		mesh->SetStaticMesh(cubeFinder.Object);
	}
}

void AAssetElement::Init(Asset* inAsset) {
	asset = inAsset;
}
