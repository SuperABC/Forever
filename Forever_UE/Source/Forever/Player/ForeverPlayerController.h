#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ForeverPlayerController.generated.h"

class UMeetOptionWidget;
class USectionSpeakingWidget;
class USectionOptionWidget;
class UPuzzleWidget;
class UPhoneWidget;
class UInputComponent;

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

	// P键(TogglePhone)绑在这里而不是Pawn的SetupPlayerInputComponent——Controller在
	// ChangeControlChange换人操控/换乘载具时不会变，是唯一能保证"任何时候按P都生效"的宿主，
	// 见.cpp里的说明和MAINCONTROLLER_TODO.md。
	virtual void SetupInputComponent() override;

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

	// 阶段5 Puzzle落地：跟前面三个Widget同一套模式，由StartPuzzleChange触发（见
	// UForeverStoryFrameworkComponent::ApplyStartPuzzle）——不需要像sectionSpeakingWidget
	// 那样在BeginPlay里手动Collapse，UPuzzleWidget自己在没有StartPuzzle之前默认不显示。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UPuzzleWidget> puzzleWidgetClass;

	UPROPERTY()
	TObjectPtr<UPuzzleWidget> puzzleWidget;

	// 阶段6 手机(Phone)系统落地：跟前面几个Widget同一套模式，由P键(TogglePhone)控制开关，
	// 不需要手动Collapse——UPhoneWidget自己在NativeConstruct里默认不显示。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UPhoneWidget> phoneWidgetClass;

	UPROPERTY()
	TObjectPtr<UPhoneWidget> phoneWidget;

	// P键(TogglePhone)的处理函数，转发给phoneWidget->TogglePhone()。
	void TogglePhone();

public:
	FORCEINLINE UMeetOptionWidget* GetMeetOptionWidget() const { return meetOptionWidget; }
	FORCEINLINE USectionSpeakingWidget* GetSectionSpeakingWidget() const { return sectionSpeakingWidget; }
	FORCEINLINE USectionOptionWidget* GetSectionOptionWidget() const { return sectionOptionWidget; }
	FORCEINLINE UPuzzleWidget* GetPuzzleWidget() const { return puzzleWidget; }
	FORCEINLINE UPhoneWidget* GetPhoneWidget() const { return phoneWidget; }
};
