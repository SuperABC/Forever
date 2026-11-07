#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ForeverHUD.generated.h"

// 瞄准准心，设计见ForeverHUD.md。
UCLASS()
class FOREVER_API AForeverHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
