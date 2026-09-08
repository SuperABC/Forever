#include "ForeverGameMode.h"

#include "ForeverCharacter.h"
#include "ForeverPlayerController.h"
#include "ForeverPlayerState.h"
#include "Framework/ForeverFrameworkActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "UObject/ConstructorHelpers.h"

AForeverGameMode::AForeverGameMode()
{
	DefaultPawnClass = AForeverCharacter::StaticClass();
	PlayerControllerClass = AForeverPlayerController::StaticClass();
	PlayerStateClass = AForeverPlayerState::StaticClass();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> planeFinder(
		TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (planeFinder.Succeeded()) {
		placeholderFloorMesh = planeFinder.Object;
	}
}

void AForeverGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 阶段0临时占位:场景里一个静态网格Actor都没有时,铺一块大平面当地板,
	// 保证空场景也能测试移动。阶段4引入真正的Map/Terrain系统后应移除。
	bool hasStaticMesh = false;
	for (TActorIterator<AStaticMeshActor> it(GetWorld()); it; ++it) {
		hasStaticMesh = true;
		break;
	}

	if (!hasStaticMesh && placeholderFloorMesh != nullptr) {
		if (AStaticMeshActor* floor = GetWorld()->SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator)) {
			floor->GetStaticMeshComponent()->SetStaticMesh(placeholderFloorMesh);
			floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
			floor->SetActorScale3D(FVector(100.f, 100.f, 1.f));
			floor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}

	EnsureFrameworkActorExists();
}

void AForeverGameMode::EnsureFrameworkActorExists()
{
	// 阶段2临时兜底:场景里一个AForeverFrameworkActor都没有时动态生成一个,
	// 保证阶段2骨架能在PIE里跑起来并触发BeginPlay日志。阶段4之后关卡里应该
	// 手动放置真实实例,再移除这段逻辑。
	for (TActorIterator<AForeverFrameworkActor> it(GetWorld()); it; ++it) {
		return;
	}

	GetWorld()->SpawnActor<AForeverFrameworkActor>(FVector::ZeroVector, FRotator::ZeroRotator);
}

AActor* AForeverGameMode::FindPlayerStart_Implementation(AController* player, const FString& incomingName)
{
	if (AActor* found = Super::FindPlayerStart_Implementation(player, incomingName)) {
		return found;
	}

	return GetWorld()->SpawnActor<APlayerStart>(FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator);
}
