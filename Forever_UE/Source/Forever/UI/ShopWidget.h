#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShopWidget.generated.h"

class UButton;
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

	UFUNCTION() void HandleBuyClicked();
	UFUNCTION() void HandleCloseClicked();

private:
	// 当前这家店所属的组织(Core对象，只在界面打开期间使用)。
	Organization* organization = nullptr;
};
