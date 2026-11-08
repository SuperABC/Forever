#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ForeverPlayerController.generated.h"

class UMeetOptionWidget;
class USectionSpeakingWidget;
class UBackgroundSpeakingWidget;
class USectionOptionWidget;
class UPuzzleWidget;
class UShopWidget;
class UPhoneWidget;
class UInventoryWidget;
class UMapWidget;
class UPausePanelWidget;
class UThrowListWidget;
class UFadeWidget;
class UForeverIdentifyComponent;
class UInputComponent;

// 玩家Controller：常驻创建全部游玩期UI Widget，承载不随Pawn变化的全局热键，见ForeverPlayerController.md。
UCLASS()
class FOREVER_API AForeverPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AForeverPlayerController();

	// 重置输入模式、加动态Mapping Context、创建全部常驻Widget。幂等；调用时机见md。
	void InitializeOnce();

protected:
	virtual void BeginPlay() override;

	// 全局热键绑在这里而不是Pawn上：换人操控/换乘载具时Controller不变，见md。
	virtual void SetupInputComponent() override;

	// 换人操控/上下载具时Controller不变但Pawn变了：刷新玩家导航线（visualize_navigation），见md。
	virtual void OnPossess(APawn* inPawn) override;

	// T键（测试）：等概率随机选一个停车场或住宅房间，显示导航线。
	void TestNavigate();

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

	// 由StartPuzzleChange触发(UForeverStoryFrameworkComponent::ApplyStartPuzzle)，自己默认不显示。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UPuzzleWidget> puzzleWidgetClass;

	UPROPERTY()
	TObjectPtr<UPuzzleWidget> puzzleWidget;

	// 商店界面，由open_shop Change触发(UForeverStoryFrameworkComponent::ApplyOpenShop)，自己默认不显示。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UShopWidget> shopWidgetClass;

	UPROPERTY()
	TObjectPtr<UShopWidget> shopWidget;

	// 由P键(TogglePhone)控制开关，自己默认不显示。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UPhoneWidget> phoneWidgetClass;

	UPROPERTY()
	TObjectPtr<UPhoneWidget> phoneWidget;

	// P键(TogglePhone)的处理函数，转发给phoneWidget->TogglePhone()。
	void TogglePhone();

	// B键(ToggleInventory)控制开关。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UInventoryWidget> inventoryWidgetClass;

	UPROPERTY()
	TObjectPtr<UInventoryWidget> inventoryWidget;

	void ToggleInventory();

	// 游戏内地图：M键(ToggleMap)开关、Esc键(CloseMap)只关不开。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMapWidget> mapWidgetClass;

	UPROPERTY()
	TObjectPtr<UMapWidget> mapWidget;

	void ToggleMap();
	void HandleCloseMap();

	// 暂停界面(保存/加载)：Tab键(TogglePause)打开；打开期间是UI Only输入，关闭由UPausePanelWidget自己处理。
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

	// 长按3的投掷物选择列表，由AForeverCharacter在长按时Open。WBP_ThrowList在BP_ForeverPlayerController的类默认值里指定。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UThrowListWidget> throwListWidgetClass;

	UPROPERTY()
	TObjectPtr<UThrowListWidget> throwListWidget;

	// 开局黑屏的类：只登记给UFadeSubsystem(控件由它持有、跨关卡存活)。WBP_Fade在BP_ForeverPlayerController的类默认值里指定。
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UFadeWidget> fadeWidgetClass;

public:
	FORCEINLINE UMeetOptionWidget* GetMeetOptionWidget() const { return meetOptionWidget; }
	FORCEINLINE USectionSpeakingWidget* GetSectionSpeakingWidget() const { return sectionSpeakingWidget; }
	FORCEINLINE UBackgroundSpeakingWidget* GetBackgroundSpeakingWidget() const { return backgroundSpeakingWidget; }
	FORCEINLINE USectionOptionWidget* GetSectionOptionWidget() const { return sectionOptionWidget; }
	FORCEINLINE UPuzzleWidget* GetPuzzleWidget() const { return puzzleWidget; }
	FORCEINLINE UShopWidget* GetShopWidget() const { return shopWidget; }
	FORCEINLINE UPhoneWidget* GetPhoneWidget() const { return phoneWidget; }
	FORCEINLINE UInventoryWidget* GetInventoryWidget() const { return inventoryWidget; }
	FORCEINLINE UMapWidget* GetMapWidget() const { return mapWidget; }
	FORCEINLINE UPausePanelWidget* GetPauseWidget() const { return pauseWidget; }
	FORCEINLINE UThrowListWidget* GetThrowListWidget() const { return throwListWidget; }};
