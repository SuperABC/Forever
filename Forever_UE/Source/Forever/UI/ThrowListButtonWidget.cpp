#include "UI/ThrowListButtonWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UThrowListButtonWidget::NativeConstruct() {
	Super::NativeConstruct();
	if (Button) Button->OnClicked.AddDynamic(this, &UThrowListButtonWidget::HandleClicked);
}

void UThrowListButtonWidget::Setup(const FString& inType, int32 inCount, bool bInActive) {
	type = inType;
	if (Label) Label->SetText(FText::FromString(FString::Printf(TEXT("%s  x%d"), *type, inCount)));
	// 激活项和其它行用不同的按钮底色，颜色是类默认值(activeColor/inactiveColor)，不需要蓝图写逻辑。
	if (Button) Button->SetBackgroundColor(bInActive ? activeColor : inactiveColor);
}

void UThrowListButtonWidget::HandleClicked() {
	onSelected.ExecuteIfBound(type);
}
