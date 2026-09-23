#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ForeverPlayerController.generated.h"

class UMeetOptionWidget;
class USectionSpeakingWidget;
class USectionOptionWidget;

// 阶段5 MeetOption UI落地：MAINCONTROLLER_TODO.md点名的"MouseScrollUp/Down + F对话选项"
// 三个热键归属这个Controller——照抄AForeverMenuController::BeginPlay的CreateWidget+
// AddToViewport模式，常驻创建一份MeetOptionWidget（不像StartMenu那样只在特定时机才创建），
// 靠列表是否为空来决定视觉上"有没有弹出"，具体Add/Remove由ACitizenElement在玩家靠近/离开
// 市民时驱动。
//
// 这次一起常驻创建SectionSpeaking/SectionOption两个剧情对话展示控件（同样的CreateWidget+
// AddToViewport模式），初始折叠隐藏，由UForeverStoryFrameworkComponent::ShowCurrentItem在
// 匹配出Dialog时按需切换显示——三个Widget的创建与生命周期完全独立，任何一个的
// xxxWidgetClass没配置只跳过那一个的创建，不影响另外两个。
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

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<USectionSpeakingWidget> sectionSpeakingWidgetClass;

	UPROPERTY()
	TObjectPtr<USectionSpeakingWidget> sectionSpeakingWidget;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<USectionOptionWidget> sectionOptionWidgetClass;

	UPROPERTY()
	TObjectPtr<USectionOptionWidget> sectionOptionWidget;

public:
	FORCEINLINE UMeetOptionWidget* GetMeetOptionWidget() const { return meetOptionWidget; }
	FORCEINLINE USectionSpeakingWidget* GetSectionSpeakingWidget() const { return sectionSpeakingWidget; }
	FORCEINLINE USectionOptionWidget* GetSectionOptionWidget() const { return sectionOptionWidget; }
};
