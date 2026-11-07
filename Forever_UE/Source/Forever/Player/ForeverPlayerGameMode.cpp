#include "ForeverPlayerGameMode.h"

#include "ForeverHUD.h"
#include "ForeverPlayerController.h"
#include "ForeverPlayerState.h"
#include "Framework/ForeverFrameworkActor.h"

#include "EngineUtils.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerStart.h"

AForeverPlayerGameMode::AForeverPlayerGameMode()
{
	// 过渡用的默认键鼠飞相机，真正的玩家角色见md。
	DefaultPawnClass = ADefaultPawn::StaticClass();
	PlayerControllerClass = AForeverPlayerController::StaticClass();
	PlayerStateClass = AForeverPlayerState::StaticClass();
	HUDClass = AForeverHUD::StaticClass(); // 瞄准准心，见ForeverHUD.h
}

void AForeverPlayerGameMode::BeginPlay()
{
	Super::BeginPlay();

	EnsureFrameworkActorExists();
}

AForeverFrameworkActor* AForeverPlayerGameMode::EnsureFrameworkActorExists()
{
	for (TActorIterator<AForeverFrameworkActor> it(GetWorld()); it; ++it) {
		return *it;
	}

	return GetWorld()->SpawnActor<AForeverFrameworkActor>(FVector::ZeroVector, FRotator::ZeroRotator);
}

AActor* AForeverPlayerGameMode::FindPlayerStart_Implementation(AController* player, const FString& incomingName)
{
	// 不能用Super的返回值是否为空判断有没有出生点:PlayerStarts为空时引擎会退化返回AWorldSettings兜底,不会返回nullptr。
	bool hasPlayerStart = false;
	for (TActorIterator<APlayerStart> it(GetWorld()); it; ++it) {
		hasPlayerStart = true;
		break;
	}

	if (hasPlayerStart) {
		if (AActor* found = Super::FindPlayerStart_Implementation(player, incomingName)) {
			UE_LOG(LogTemp, Log, TEXT("AForeverPlayerGameMode::FindPlayerStart_Implementation: found existing PlayerStart '%s' at %s, using it instead of map-center fallback."),
				*found->GetName(), *found->GetActorLocation().ToString());
			return found;
		}
	}

	// 找出生点时地形还没生成(世界生成分阶段，见AForeverFrameworkActor::BeginPlay)，只能用地图中心的常量位置当过渡。
	FVector spawnLocation = AForeverFrameworkActor::GetDefaultSpawnLocation();

	UE_LOG(LogTemp, Log, TEXT("AForeverPlayerGameMode::FindPlayerStart_Implementation: no PlayerStart in level, spawning at map center %s."),
		*spawnLocation.ToString());
	return GetWorld()->SpawnActor<APlayerStart>(spawnLocation, FRotator::ZeroRotator);
}
