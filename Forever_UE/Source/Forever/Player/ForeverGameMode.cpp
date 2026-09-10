#include "ForeverGameMode.h"

#include "ForeverCharacter.h"
#include "ForeverPlayerController.h"
#include "ForeverPlayerState.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverTerrainFrameworkComponent.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

AForeverGameMode::AForeverGameMode()
{
	DefaultPawnClass = AForeverCharacter::StaticClass();
	PlayerControllerClass = AForeverPlayerController::StaticClass();
	PlayerStateClass = AForeverPlayerState::StaticClass();
}

void AForeverGameMode::BeginPlay()
{
	Super::BeginPlay();

	EnsureFrameworkActorExists();
}

AForeverFrameworkActor* AForeverGameMode::EnsureFrameworkActorExists()
{
	for (TActorIterator<AForeverFrameworkActor> it(GetWorld()); it; ++it) {
		it->EnsureMapGenerated();
		return *it;
	}

	AForeverFrameworkActor* spawned = GetWorld()->SpawnActor<AForeverFrameworkActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (spawned) {
		spawned->EnsureMapGenerated();
	}
	return spawned;
}

AActor* AForeverGameMode::FindPlayerStart_Implementation(AController* player, const FString& incomingName)
{
	// 只有关卡里真的有手动放置的APlayerStart才交给引擎默认逻辑处理——不能用
	// "Super::FindPlayerStart_Implementation的返回值是否为空"来判断"有没有真正的出生点",
	// 因为引擎自己的ChoosePlayerStart_Implementation在PlayerStarts数组为空时会退化返回
	// AWorldSettings(每个关卡都有、永远在原点)兜底,不会返回nullptr。之前就是被这个坑
	// 绕过去了:哪怕关卡里一个PlayerStart都没有,Super那个if分支也一直是"命中"的,
	// 地图正中心这段代码从来没机会跑到过。
	bool hasPlayerStart = false;
	for (TActorIterator<APlayerStart> it(GetWorld()); it; ++it) {
		hasPlayerStart = true;
		break;
	}

	if (hasPlayerStart) {
		if (AActor* found = Super::FindPlayerStart_Implementation(player, incomingName)) {
			UE_LOG(LogTemp, Log, TEXT("AForeverGameMode::FindPlayerStart_Implementation: found existing PlayerStart '%s' at %s, using it instead of map-center fallback."),
				*found->GetName(), *found->GetActorLocation().ToString());
			return found;
		}
	}

	// 地图正中心作为出生点(要求#5)。EnsureFrameworkActorExists是幂等的,不论BeginPlay和这里
	// 谁先跑到,这一步结束时地形一定已经生成好。
	FVector spawnLocation(0.f, 0.f, 100.f);
	if (AForeverFrameworkActor* framework = EnsureFrameworkActorExists()) {
		if (UForeverTerrainFrameworkComponent* terrain = framework->GetTerrainFramework()) {
			if (terrain->IsTerrainGenerated()) {
				spawnLocation = terrain->GetMapCenterWorldLocation();
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AForeverGameMode::FindPlayerStart_Implementation: no PlayerStart in level, spawning at map center %s."),
		*spawnLocation.ToString());
	return GetWorld()->SpawnActor<APlayerStart>(spawnLocation, FRotator::ZeroRotator);
}
