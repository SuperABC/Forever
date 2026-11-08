#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShopRowWidget.generated.h"

class UTextBlock;
class UButton;
class UEditableTextBox;

// 商店列表里的一行：商品类型、价格、库存，右边是数量输入框和+-按钮，见ShopRowWidget.md。
UCLASS()
class FOREVER_API UShopRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 价格单位是分；数量变化(按钮或直接输入)时调OnChanged。
	void SetupRow(const FString& InType, int64 InPriceCents, int32 InStock, TFunction<void()> InOnChanged);

	int32 GetQuantity() const { return quantity; }

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TypeText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> PriceText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> StockText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UEditableTextBox> QuantityBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> MinusButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> PlusButton;

	UFUNCTION() void HandleMinusClicked();
	UFUNCTION() void HandlePlusClicked();
	UFUNCTION() void HandleQuantityChanged(const FText& Text);

private:
	void SetQuantity(int32 value);

	int32 quantity = 0;
	int32 stock = 0;
	TFunction<void()> onChanged;
};
