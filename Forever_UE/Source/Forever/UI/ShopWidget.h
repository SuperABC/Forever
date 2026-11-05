#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShopWidget.generated.h"

class UButton;
class Organization;

// 商店界面：玩家和店员对话选"购买汉堡"后，由open_shop Change打开(见
// UForeverStoryFrameworkComponent::ApplyOpenShop)。现在只有一个购买按钮，按下去固定买一个汉堡，
// 钱够不够、背包放不放得下都在Core里判断(Economy::PlayerPurchase)，结果打在屏幕左上角的调试
// 信息里。以后界面会显示这个组织内部仓库里所有能买的东西，所以打开时就记下店员所属的组织。
//
// 输入模式切换照抄UPausePanelWidget：打开时UI Only+显示鼠标，所以关闭键(Esc)要在
// NativeOnKeyDown里自己拦；不改全局时间缩放(城市继续运行)。
UCLASS()
class FOREVER_API UShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 打开并记住这次买东西付款给哪个组织。
	void OpenShop(Organization* inOrganization);
	void CloseShop();
	bool IsOpen() const { return GetVisibility() == ESlateVisibility::Visible; }

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 蓝图里必须有一个名为BuyButton的按钮；CloseButton可选。
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> BuyButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CloseButton;

	UFUNCTION() void HandleBuyClicked();
	UFUNCTION() void HandleCloseClicked();

private:
	// 当前这家店所属的组织(Core对象，只在界面打开期间使用)。
	Organization* organization = nullptr;
};
