#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ForeverMenuController.generated.h"

class UUserWidget;
class UFadeWidget;

UCLASS()
class FOREVER_API AForeverMenuController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> startMenuWidgetClass;

	// 开局黑屏的类，只登记给UFadeSubsystem：按下开始/加载按钮的那一刻就要变黑(OpenLevel要
	// 同步加载很久)，所以Menu关卡也要登记。WBP_Fade在BP_ForeverMenuController的类默认值里指定。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UFadeWidget> fadeWidgetClass;
};
