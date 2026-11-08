#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "society/economy.h"
#include "ShopWidget.generated.h"

class UButton;
class UTextBlock;
class UScrollBox;
class UShopRowWidget;
class Organization;

// 商店界面，由open_shop Change打开(UForeverStoryFrameworkComponent::ApplyOpenShop)，见ShopWidget.md。
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

	// 商品列表(每种商品一行)和底部的汇总文字；超限的数字会标红。
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ShopNameText; // 显示组织名
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UScrollBox> RowScrollBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TotalCountText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TotalVolumeText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TotalPriceText;

	// 每行的蓝图类，需要在WBP_Shop的默认值里指定。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop") TSubclassOf<UShopRowWidget> ShopRowClass;

	UFUNCTION() void HandleBuyClicked();
	UFUNCTION() void HandleCloseClicked();

private:
	// 按各行数量重算个数/体积/总价并标红；返回购物车能否购买(有商品、放得下、付得起)。
	bool Refresh();

	// 当前这家店所属的组织(Core对象，只在界面打开期间使用)。
	Organization* organization = nullptr;

	// 当前打开时列出的商品和对应的行(下标一一对应)。
	std::vector<Economy::ShopItem> items;
	TArray<TObjectPtr<UShopRowWidget>> rows;

	FSlateColor normalColor;

	// 打开商店时暂停世界(全局时间膨胀置0)，关闭时还原成打开前的值。
	float previousDilation = 1.f;
};
