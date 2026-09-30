#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ForeverHUD.generated.h"

// 瞄准准心：用AHUD原生Canvas画线，不新建UMG Widget/不需要在编辑器里另外配一份Blueprint
// 资产——瞄准与否是每帧都可能变化的状态，原生DrawHUD()里判断一次比维护一个UUserWidget的
// 可见性开关更直接，见.cpp。
UCLASS()
class FOREVER_API AForeverHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
