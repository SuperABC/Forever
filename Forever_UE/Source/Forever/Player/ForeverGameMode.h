#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ForeverGameMode.generated.h"

class AForeverFrameworkActor;

UCLASS()
class FOREVER_API AForeverGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AForeverGameMode();

	virtual void BeginPlay() override;
	virtual AActor* FindPlayerStart_Implementation(AController* player, const FString& incomingName) override;

protected:
	// 阶段2临时兜底:场景里没有手动放置AForeverFrameworkActor时动态生成一个,顺带确保它的
	// 地形已生成(EnsureTerrainGenerated幂等,重复调用无副作用)。一旦关卡里手动放置了真实
	// 实例应移除这段查找/生成逻辑。BeginPlay和FindPlayerStart_Implementation都会调用它,
	// 保证不论两者实际调用顺序如何,出生点计算时地形都已经生成好。
	AForeverFrameworkActor* EnsureFrameworkActorExists();
};
