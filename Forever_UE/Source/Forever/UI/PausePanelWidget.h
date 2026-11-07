#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PausePanelWidget.generated.h"

class UButton;
class UTextBlock;

// 游戏内暂停界面：打开时全局时间缩放设0，提供保存/加载/退出，详见PausePanelWidget.md。
UCLASS()
class FOREVER_API UPausePanelWidget : public UUserWidget
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
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;
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

	// 可选：状态文本框(饱食度/金钱)，见md。暂停时NativeTick不受时间缩放影响，打开期间每帧刷新。
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;

	UFUNCTION() void HandleSaveClicked();
	UFUNCTION() void HandleLoadClicked();
	UFUNCTION() void HandleCloseClicked();
	UFUNCTION() void HandleExitClicked();

private:
	void RefreshStatus();
	void ShowMessage(const FString& message, bool bError);
	// 文件对话框关掉之后键盘焦点不一定回到这个widget，重新抢一次，Tab/Esc才能继续关界面。
	void RefocusSelf();
};
