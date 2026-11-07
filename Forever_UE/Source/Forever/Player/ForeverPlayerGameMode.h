#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ForeverPlayerGameMode.generated.h"

class AForeverFrameworkActor;

UCLASS()
class FOREVER_API AForeverPlayerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AForeverPlayerGameMode();

	virtual void BeginPlay() override;
	virtual AActor* FindPlayerStart_Implementation(AController* player, const FString& incomingName) override;

protected:
	// 临时兜底:场景里没有手动放置AForeverFrameworkActor时动态生成一个。
	AForeverFrameworkActor* EnsureFrameworkActorExists();
};
