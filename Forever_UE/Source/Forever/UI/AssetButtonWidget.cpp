#include "UI/AssetButtonWidget.h"
#include "UI/InventoryWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

#include "player/asset.h"

void UAssetButtonWidget::Setup(UForeverInventoryWidget* inOwner, Asset* inAsset, EInvZone inZone, Asset* inParent) {
	owner = inOwner;
	asset = inAsset;
	zone = inZone;
	parentContainer = inParent;
	bIsBackRow = false;
	lastClickTime = -1000.0;
	SetRowSelected(false);
	bHovered = false; // 行对象被复用且可能刚好在鼠标下，悬停状态等下一次MouseEnter再置
	UpdateButtonState(false);

	if (Text && asset) {
		Text->SetText(FText::FromString(UForeverInventoryWidget::DescribeAsset(asset, asset->GetCount())));
	}
}

void UAssetButtonWidget::SetupAsBack(UForeverInventoryWidget* inOwner, EInvZone inZone) {
	owner = inOwner;
	asset = nullptr;
	zone = inZone;
	parentContainer = nullptr;
	bIsBackRow = true;
	lastClickTime = -1000.0;
	SetRowSelected(false);
	bHovered = false; // 行对象被复用且可能刚好在鼠标下，悬停状态等下一次MouseEnter再置
	UpdateButtonState(false);

	if (Text) Text->SetText(FText::FromString(TEXT(".. (返回上一级)")));
}

void UAssetButtonWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (Button) {
		styleState.Capture(Button); // 必须在改可见性之前记下WBP里配置的样式
		Button->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UAssetButtonWidget::SetLabel(const FString& label) {
	if (Text) Text->SetText(FText::FromString(label));
}

void UAssetButtonWidget::UpdateButtonState(bool bPressed) {
	styleState.Apply(Button, bPressed ? FInvButtonStyleState::EState::Pressed
		: bHovered ? FInvButtonStyleState::EState::Hovered : FInvButtonStyleState::EState::Normal);
}

void UAssetButtonWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	UpdateButtonState(false);
}

void UAssetButtonWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent) {
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	UpdateButtonState(false);
}

void UAssetButtonWidget::SetRowSelected(bool bSelected) {
	if (Button) {
		Button->SetBackgroundColor(bSelected ? FLinearColor::Yellow : FLinearColor::White);
	}
}

FReply UAssetButtonWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	UForeverInventoryWidget* ownerWidget = owner.Get();
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !ownerWidget) {
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	UpdateButtonState(true);
	// 面板捕获鼠标，没拖动就松开 = Click()，拖动超过阈值 = 面板的拖拽。
	return ownerWidget->BeginRowPress(this, InMouseEvent);
}

void UAssetButtonWidget::FinishPress(const FVector2D& screenPosition) {
	bHovered = IsUnderScreenPosition(screenPosition);
	UpdateButtonState(false);
}

void UAssetButtonWidget::Click() {
	UForeverInventoryWidget* ownerWidget = owner.Get();
	if (!ownerWidget) return;

	if (bIsBackRow) {
		// "返回上一级"不代表具体资产，单击直接退出就够，不需要区分单双击。
		ownerWidget->ExitRow(this);
		return;
	}

	double now = FPlatformTime::Seconds();
	bool bIsDoubleClick = (now - lastClickTime) <= kDoubleClickInterval;
	lastClickTime = now;

	if (bIsDoubleClick && asset && asset->IsContainer()) {
		ownerWidget->EnterRow(this);
	} else {
		ownerWidget->SelectRow(this);
	}
}
