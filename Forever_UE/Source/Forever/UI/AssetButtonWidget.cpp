#include "UI/AssetButtonWidget.h"
#include "UI/InventoryWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

#include "player/asset.h"

void UAssetButtonWidget::Setup(UForeverInventoryWidget* inOwner, Asset* inAsset, bool inIsRoomSide) {
	owner = inOwner;
	asset = inAsset;
	isRoomSide = inIsRoomSide;
	bIsBackRow = false;

	if (Text && asset) {
		FString label = UTF8_TO_TCHAR(asset->GetType().c_str());
		if (asset->GetCount() != 1) label += FString::Printf(TEXT(" x%d"), asset->GetCount());
		if (asset->IsContainer()) label += TEXT(" [容器]");
		Text->SetText(FText::FromString(label));
	}
}

void UAssetButtonWidget::SetupAsBack(UForeverInventoryWidget* inOwner, bool inIsRoomSide) {
	owner = inOwner;
	asset = nullptr;
	isRoomSide = inIsRoomSide;
	bIsBackRow = true;

	if (Text) Text->SetText(FText::FromString(TEXT(".. (返回上一级)")));
}

void UAssetButtonWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (Button) {
		Button->OnClicked.AddDynamic(this, &UAssetButtonWidget::HandleClicked);
	}
}

void UAssetButtonWidget::SetRowSelected(bool bSelected) {
	if (Button) {
		Button->SetBackgroundColor(bSelected ? FLinearColor::Yellow : FLinearColor::White);
	}
}

void UAssetButtonWidget::HandleClicked() {
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
