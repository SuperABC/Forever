#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ForeverPlayerController.generated.h"

class UMeetOptionWidget;
class USectionSpeakingWidget;
class UBackgroundSpeakingWidget;
class USectionOptionWidget;
class UPuzzleWidget;
class UPhoneWidget;
class UInventoryWidget;
class UMapWidget;
class UPausePanelWidget;
class UThrowListWidget;
class UFadeWidget;
class UForeverIdentifyComponent;
class UInputComponent;

// 旧蓝图MainController的"MouseScrollUp/Down + F对话选项"三个热键归属这个Controller(见ForeverPlayerController.md)
// ——照抄AForeverMenuController::BeginPlay的CreateWidget+AddToViewport模式，常驻创建一份
// MeetOptionWidget（不像StartMenu那样只在特定时机才创建），靠列表是否为空来决定视觉上
// "有没有弹出"，具体Add/Remove由ACitizenElement在玩家靠近/离开市民时驱动。
//
// 同一套模式再常驻创建SectionSpeaking/SectionOption两个剧情对话展示控件，初始折叠隐藏，
// 由UForeverStoryFrameworkComponent::ShowCurrentItem在匹配出Dialog时按需切换显示——三个
// Widget的创建与生命周期完全独立，任何一个的xxxWidgetClass没配置只跳过那一个的创建，不
// 影响另外两个。
UCLASS()
class FOREVER_API AForeverPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AForeverPlayerController();

	// 重置输入模式、加动态Mapping Context、创建全部常驻Widget。幂等：只有第一次调用生效。
	// BeginPlay会调，AForeverFrameworkActor::BeginPlay在生成世界/广播game_start之前也会调
	// (打包版里它先于Controller的BeginPlay)，见.cpp里的说明。
	void InitializeOnce();

protected:
	virtual void BeginPlay() override;

	// P键(TogglePhone)绑在这里而不是Pawn的SetupPlayerInputComponent——Controller在
	// ChangeControlChange换人操控/换乘载具时不会变，是唯一能保证"任何时候按P都生效"的宿主，
	// 见.cpp里的说明和ForeverPlayerController.md"旧MainController热键"一节。
	virtual void SetupInputComponent() override;

	bool bInitialized = false;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMeetOptionWidget> meetOptionWidgetClass;

	UPROPERTY()
	TObjectPtr<UMeetOptionWidget> meetOptionWidget;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<USectionSpeakingWidget> sectionSpeakingWidgetClass;

	UPROPERTY()
	TObjectPtr<USectionSpeakingWidget> sectionSpeakingWidget;

	// 后台对话(屏幕上方、不抢操控、计时结束)，见UForeverStoryFrameworkComponent::ShowCurrentBackground。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UBackgroundSpeakingWidget> backgroundSpeakingWidgetClass;

	UPROPERTY()
	TObjectPtr<UBackgroundSpeakingWidget> backgroundSpeakingWidget;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<USectionOptionWidget> sectionOptionWidgetClass;

	UPROPERTY()
	TObjectPtr<USectionOptionWidget> sectionOptionWidget;

	// 跟前面三个Widget同一套模式，由StartPuzzleChange触发（见
	// UForeverStoryFrameworkComponent::ApplyStartPuzzle）——不需要像sectionSpeakingWidget
	// 那样在BeginPlay里手动Collapse，UPuzzleWidget自己在没有StartPuzzle之前默认不显示。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UPuzzleWidget> puzzleWidgetClass;

	UPROPERTY()
	TObjectPtr<UPuzzleWidget> puzzleWidget;

	// 跟前面几个Widget同一套模式，由P键(TogglePhone)控制开关，不需要手动Collapse——
	// UPhoneWidget自己在NativeConstruct里默认不显示。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UPhoneWidget> phoneWidgetClass;

	UPROPERTY()
	TObjectPtr<UPhoneWidget> phoneWidget;

	// P键(TogglePhone)的处理函数，转发给phoneWidget->TogglePhone()。
	void TogglePhone();

	// 跟前面几个Widget同一套模式(常驻创建，B键(ToggleInventory)控制开关)，见
	// Source/Forever/UI/InventoryWidget.h、ForeverPlayerController.md"旧MainController热键"一节。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UInventoryWidget> inventoryWidgetClass;

	UPROPERTY()
	TObjectPtr<UInventoryWidget> inventoryWidget;

	void ToggleInventory();

	// 游戏内地图：跟前面几个Widget同一套模式(常驻创建)，M键(ToggleMap)开关、Esc键
	// (CloseMap)只关不开，见Source/Forever/UI/MapWidget.h。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMapWidget> mapWidgetClass;

	UPROPERTY()
	TObjectPtr<UMapWidget> mapWidget;

	void ToggleMap();
	void HandleCloseMap();

	// 暂停界面(保存/加载)：跟前面几个Widget同一套模式(常驻创建)，Tab键(TogglePause)打开；
	// 界面打开期间是UI Only输入，关闭由UPausePanelWidget自己的NativeOnKeyDown处理。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UPausePanelWidget> pauseWidgetClass;

	UPROPERTY()
	TObjectPtr<UPausePanelWidget> pauseWidget;

	// 地图/手机/背包/谜题/剧情对话任何一个开着时不打开暂停界面(它们各自占着UI Only输入和
	// 键盘焦点，叠上去会互相抢)。
	void TogglePause();

	// 识别模式(按住鼠标中键)：热键绑在Controller上(换人/上下载具时Controller不变)，实际逻辑在
	// UForeverIdentifyComponent里。
	UPROPERTY(VisibleAnywhere, Category = "Identify")
	TObjectPtr<UForeverIdentifyComponent> identifyComponent;

	void EnterIdentify();
	void ExitIdentify();

	// 长按3的投掷物选择列表：跟前面几个Widget同一套模式(常驻创建、初始折叠)，由AForeverCharacter
	// 在长按时Open。WBP_ThrowList在BP_ForeverPlayerController的类默认值里指定。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UThrowListWidget> throwListWidgetClass;

	UPROPERTY()
	TObjectPtr<UThrowListWidget> throwListWidget;

	// 开局黑屏的类：只是登记给UFadeSubsystem(控件本身由它持有、跨关卡存活)，见
	// UI/FadeSubsystem.h。WBP_Fade在BP_ForeverPlayerController的类默认值里指定。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UFadeWidget> fadeWidgetClass;

public:
	FORCEINLINE UMeetOptionWidget* GetMeetOptionWidget() const { return meetOptionWidget; }
	FORCEINLINE USectionSpeakingWidget* GetSectionSpeakingWidget() const { return sectionSpeakingWidget; }
	FORCEINLINE UBackgroundSpeakingWidget* GetBackgroundSpeakingWidget() const { return backgroundSpeakingWidget; }
	FORCEINLINE USectionOptionWidget* GetSectionOptionWidget() const { return sectionOptionWidget; }
	FORCEINLINE UPuzzleWidget* GetPuzzleWidget() const { return puzzleWidget; }
	FORCEINLINE UPhoneWidget* GetPhoneWidget() const { return phoneWidget; }
	FORCEINLINE UInventoryWidget* GetInventoryWidget() const { return inventoryWidget; }
	FORCEINLINE UMapWidget* GetMapWidget() const { return mapWidget; }
	FORCEINLINE UPausePanelWidget* GetPauseWidget() const { return pauseWidget; }
	FORCEINLINE UThrowListWidget* GetThrowListWidget() const { return throwListWidget; }};
