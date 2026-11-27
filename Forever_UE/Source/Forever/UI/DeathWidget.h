#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeathWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

// 玩家死亡界面(WBP_Death的C++基类)：慢镜头期间画面渐暗、标题渐显，结束后出现"继续游戏"/"退出游戏"两个按钮。
// 流程由UForeverDeathFrameworkComponent驱动，这里只管显示和把按钮点击转回去，见DeathWidget.md。
UCLASS()
class FOREVER_API UDeathWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 刚死：显示出来(不拦截鼠标)，遮罩全透明、按钮隐藏。
	void BeginDeath();
	// 渐暗进度0..1：遮罩不透明度=progress×dimOpacity，标题不透明度=progress。
	void SetProgress(float progress);
	// 慢镜头结束：出现两个按钮，接管鼠标/键盘(UI Only)。
	void ShowMenu();
	// 点了继续游戏：按钮收起、交还游戏输入，遮罩保持(随后被跳时间的黑屏盖住)。
	void HideMenu();
	// 整个流程结束(复活了/黑屏已经盖住)：隐藏。
	void EndDeath();

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 渐暗结束时遮罩的不透明度(0..1)，1=全黑。
	UPROPERTY(EditAnywhere, Category = "Death")
	float dimOpacity = 0.75f;

	// 全屏黑色遮罩(Image，颜色设成黑色、铺满)。可选：没放就不渐暗(只有慢镜头和褪色)。
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> DimImage;
	// 标题文字(比如"你死了")，跟着渐暗一起渐显。可选。
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ContinueButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ExitButton;

	UFUNCTION() void HandleContinueClicked();
	UFUNCTION() void HandleExitClicked();

private:
	void SetButtonsVisible(bool bVisible);
	class UForeverDeathFrameworkComponent* FindDeathFramework() const;
};
