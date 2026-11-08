#include "UI/ShopRowWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

void UShopRowWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (MinusButton) MinusButton->OnClicked.AddDynamic(this, &UShopRowWidget::HandleMinusClicked);
	if (PlusButton) PlusButton->OnClicked.AddDynamic(this, &UShopRowWidget::HandlePlusClicked);
	if (QuantityBox) QuantityBox->OnTextChanged.AddDynamic(this, &UShopRowWidget::HandleQuantityChanged);
}

void UShopRowWidget::SetupRow(const FString& InType, int64 InPriceCents, int32 InStock, TFunction<void()> InOnChanged) {
	stock = InStock;
	onChanged = MoveTemp(InOnChanged);
	quantity = 0;

	if (TypeText) TypeText->SetText(FText::FromString(InType));
	if (PriceText) PriceText->SetText(FText::FromString(FString::Printf(TEXT("%.2f元"), InPriceCents / 100.0)));
	if (StockText) StockText->SetText(FText::AsNumber(stock));
	if (QuantityBox) QuantityBox->SetText(FText::AsNumber(0));
}

void UShopRowWidget::SetQuantity(int32 value) {
	value = FMath::Clamp(value, 0, stock);
	const bool changed = value != quantity;
	quantity = value;

	// 文本框内容和夹紧后的数量不一致时(包括输入了非数字)回写；相同则不动，避免打断输入。
	if (QuantityBox && QuantityBox->GetText().ToString() != FString::FromInt(quantity)) {
		QuantityBox->SetText(FText::AsNumber(quantity));
	}
	if (changed && onChanged) onChanged();
}

void UShopRowWidget::HandleMinusClicked() {
	SetQuantity(quantity - 1);
}

void UShopRowWidget::HandlePlusClicked() {
	SetQuantity(quantity + 1);
}

void UShopRowWidget::HandleQuantityChanged(const FText& Text) {
	const FString raw = Text.ToString();
	// 允许输入过程中的空串，当作0，但不回写空串。
	if (raw.IsEmpty()) {
		const bool changed = quantity != 0;
		quantity = 0;
		if (changed && onChanged) onChanged();
		return;
	}
	SetQuantity(FCString::Atoi(*raw));
}
