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
	// 开局不再生成一个独立于Populace之外的"玩家角色"——先用引擎自带的ADefaultPawn
	// (纯UE默认键鼠飞相机，legacy input，见Config/DefaultInput.ini补的AxisMappings)过渡，
	// 真正"玩家是谁"这件事交给Story系统：test.json的game_start会通过ChangeControlChange
	// 把控制权切换到一个随机市民(ACitizenElement，见ForeverStoryFrameworkComponent.cpp)。
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
			UE_LOG(LogTemp, Log, TEXT("AForeverPlayerGameMode::FindPlayerStart_Implementation: found existing PlayerStart '%s' at %s, using it instead of map-center fallback."),
				*found->GetName(), *found->GetActorLocation().ToString());
			return found;
		}
	}

	// 地图正中心作为出生点(要求#5)。世界生成现在拆成每帧一个阶段(见AForeverFrameworkActor::BeginPlay)，
	// 找出生点时地形还没生成，所以只用地图中心的常量位置当过渡——玩家真正的位置由game_start的
	// change_control/RestoreControlledPawn决定，没人接管时生成完成后会被挪到地形上方。
	FVector spawnLocation = AForeverFrameworkActor::GetDefaultSpawnLocation();

	UE_LOG(LogTemp, Log, TEXT("AForeverPlayerGameMode::FindPlayerStart_Implementation: no PlayerStart in level, spawning at map center %s."),
		*spawnLocation.ToString());
	return GetWorld()->SpawnActor<APlayerStart>(spawnLocation, FRotator::ZeroRotator);
}
