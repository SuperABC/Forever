#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ForeverGameMode.generated.h"

class UStaticMesh;

UCLASS()
class FOREVER_API AForeverGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AForeverGameMode();

	virtual void BeginPlay() override;
	virtual AActor* FindPlayerStart_Implementation(AController* player, const FString& incomingName) override;

protected:
	// 阶段0临时占位:场景里没有可行走地面时生成一块大平面。
	// 阶段4 Map/Terrain系统落地后应移除。
	UPROPERTY(EditDefaultsOnly, Category = "Placeholder")
	TObjectPtr<UStaticMesh> placeholderFloorMesh;
};
