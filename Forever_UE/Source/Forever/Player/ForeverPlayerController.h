#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ForeverPlayerController.generated.h"

class UMeetOptionWidget;

// 阶段5 MeetOption UI落地：MAINCONTROLLER_TODO.md点名的"MouseScrollUp/Down + F对话选项"
// 三个热键归属这个Controller——照抄AForeverMenuController::BeginPlay的CreateWidget+
// AddToViewport模式，常驻创建一份MeetOptionWidget（不像StartMenu那样只在特定时机才创建），
// 靠列表是否为空来决定视觉上"有没有弹出"，具体Add/Remove由ACitizenElement在玩家靠近/离开
// 市民时驱动。
UCLASS()
class FOREVER_API AForeverPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMeetOptionWidget> meetOptionWidgetClass;

	UPROPERTY()
	TObjectPtr<UMeetOptionWidget> meetOptionWidget;

public:
	FORCEINLINE UMeetOptionWidget* GetMeetOptionWidget() const { return meetOptionWidget; }
};
