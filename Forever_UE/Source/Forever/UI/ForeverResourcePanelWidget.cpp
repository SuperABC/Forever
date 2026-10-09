#include "ForeverResourcePanelWidget.h"

#include "ForeverPathRowWidget.h"
#include "Mod/ForeverConfigBridgeSubsystem.h"

#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "ForeverScrollBoxUtils.h"

using ForeverScrollBoxUtils::AddChildWithGap;

void UForeverResourcePanelWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (AddResourceButton) {
		AddResourceButton->OnClicked.AddDynamic(this, &UForeverResourcePanelWidget::HandleAddResourceClicked);
	}

	RefreshRows();
}

void UForeverResourcePanelWidget::RefreshRows() {
	if (ResourceList) {
		ResourceList->ClearChildren();
	}

	if (!PathRowClass || !ResourceList) {
		return;
	}

	UGameInstance* gameInstance = GetGameInstance();
	UForeverConfigBridgeSubsystem* bridge = gameInstance ? gameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>() : nullptr;
	if (!bridge) {
		return;
	}

	for (const FString& path : bridge->GetResourceRootPaths()) {
		UForeverPathRowWidget* rowWidget = CreateWidget<UForeverPathRowWidget>(this, PathRowClass);
		if (!rowWidget) {
			continue;
		}

		rowWidget->SetupRow(path, [this](const FString& deletedPath) {
			UGameInstance* innerGameInstance = GetGameInstance();
			UForeverConfigBridgeSubsystem* innerBridge = innerGameInstance ? innerGameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>() : nullptr;
			if (innerBridge) {
				innerBridge->RemoveResourcePath(deletedPath);
			}
			RefreshRows();
		});
		AddChildWithGap(ResourceList, rowWidget);
	}
}

void UForeverResourcePanelWidget::HandleAddResourceClicked() {
	UGameInstance* gameInstance = GetGameInstance();
	UForeverConfigBridgeSubsystem* bridge = gameInstance ? gameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>() : nullptr;
	if (!bridge) {
		return;
	}

	FString selected;
	if (bridge->SelectFolder(selected)) {
		bridge->AddResourcePath(selected);
		RefreshRows();
	}
}
