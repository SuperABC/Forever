#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForeverPausePanelWidget.generated.h"

class UButton;
class UTextBlock;

// 游戏内暂停界面：Tab键(TogglePause，可在KeyBindings.ini里改)打开/关闭，打开时把全局时间
// 缩放设成0暂停世界(时间缩放已经是0时——LOD冻结/谜题——不允许打开)，提供保存/加载两个
// 按钮，各自弹出文件对话框，实际读写交给UForeverSaveSubsystem。由AForeverPlayerController常驻创建，见那边的TogglePause。
//
// 打开/关闭的输入模式切换照抄UMapWidget：打开时UI Only+显示鼠标，Enhanced Input收不到按键，
// 所以关闭键(Tab/Esc)要在NativeOnKeyDown里自己拦。详见ForeverPausePanelWidget.md。
UCLASS()
class FOREVER_API UForeverPausePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 可见就ClosePanel，不可见就OpenPanel。
	void TogglePanel();
	void OpenPanel();
	void ClosePanel();
	bool IsOpen() const { return GetVisibility() == ESlateVisibility::Visible; }

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> SaveButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> LoadButton;

	// 关闭按钮：和Tab/Esc一样关闭界面、继续游戏。
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CloseButton;

	// 退出游戏：关掉暂停、销毁当前世界(AForeverFrameworkActor::EndPlay释放Core对象)，回到主菜单关卡。
	// BindWidgetOptional：WBP_PausePanel里还没放这个按钮时不影响蓝图编译。
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> ExitButton;

	// 可选：显示保存成功/失败原因的文字。没放的话结果打在屏幕左上角的调试信息里。
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MessageText;

	UFUNCTION() void HandleSaveClicked();
	UFUNCTION() void HandleLoadClicked();
	UFUNCTION() void HandleCloseClicked();
	UFUNCTION() void HandleExitClicked();

private:
	void ShowMessage(const FString& message, bool bError);
	// 文件对话框关掉之后键盘焦点不一定回到这个widget，重新抢一次，Tab/Esc才能继续关界面。
	void RefocusSelf();
};
