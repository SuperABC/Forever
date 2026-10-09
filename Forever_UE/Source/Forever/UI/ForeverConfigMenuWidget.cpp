#include "ForeverConfigMenuWidget.h"

#include "ForeverStoryPanelWidget.h"
#include "ForeverModPanelWidget.h"
#include "ForeverResourcePanelWidget.h"
#include "Mod/ForeverConfigBridgeSubsystem.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UForeverConfigMenuWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (StoryTabButton) {
		StoryTabButton->OnClicked.AddDynamic(this, &UForeverConfigMenuWidget::HandleStoryTabClicked);
	}
	if (ModTabButton) {
		ModTabButton->OnClicked.AddDynamic(this, &UForeverConfigMenuWidget::HandleModTabClicked);
	}
	if (ResourceTabButton) {
		ResourceTabButton->OnClicked.AddDynamic(this, &UForeverConfigMenuWidget::HandleResourceTabClicked);
	}
	if (StartButton) {
		StartButton->OnClicked.AddDynamic(this, &UForeverConfigMenuWidget::HandleStartClicked);
	}
	if (FailureMessage) {
		FailureMessage->SetVisibility(ESlateVisibility::Collapsed);
	}

	HandleStoryTabClicked();
}

void UForeverConfigMenuWidget::ShowOnly(ETab Tab) {
	if (StoryPanel) {
		StoryPanel->SetVisibility(Tab == ETab::Story ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ModPanel) {
		ModPanel->SetVisibility(Tab == ETab::Mod ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ResourcePanel) {
		ResourcePanel->SetVisibility(Tab == ETab::Resource ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UForeverConfigMenuWidget::HandleStoryTabClicked() {
	ShowOnly(ETab::Story);
	if (StoryPanel) {
		StoryPanel->RefreshRows();
	}
}

void UForeverConfigMenuWidget::HandleModTabClicked() {
	ShowOnly(ETab::Mod);
	if (ModPanel) {
		ModPanel->RefreshDllList();
	}
}

void UForeverConfigMenuWidget::HandleResourceTabClicked() {
	ShowOnly(ETab::Resource);
	if (ResourcePanel) {
		ResourcePanel->RefreshRows();
	}
}

void UForeverConfigMenuWidget::HandleStartClicked() {
	UGameInstance* gameInstance = GetGameInstance();
	UForeverConfigBridgeSubsystem* bridge = gameInstance ? gameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>() : nullptr;
	if (!bridge) {
		return;
	}

	FString failureMessage;
	if (!bridge->ValidateAndStartGame(failureMessage)) {
		if (FailureMessage) {
			FailureMessage->SetText(FText::FromString(failureMessage));
			FailureMessage->SetVisibility(ESlateVisibility::Visible);
		}
	}
}
