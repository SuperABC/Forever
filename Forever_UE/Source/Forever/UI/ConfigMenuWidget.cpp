#include "ConfigMenuWidget.h"

#include "StoryPanelWidget.h"
#include "ModPanelWidget.h"
#include "ResourcePanelWidget.h"
#include "Mod/ForeverConfigBridgeSubsystem.h"
#include "Mod/ForeverSaveSubsystem.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UConfigMenuWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (StoryTabButton) {
		StoryTabButton->OnClicked.AddDynamic(this, &UConfigMenuWidget::HandleStoryTabClicked);
	}
	if (ModTabButton) {
		ModTabButton->OnClicked.AddDynamic(this, &UConfigMenuWidget::HandleModTabClicked);
	}
	if (ResourceTabButton) {
		ResourceTabButton->OnClicked.AddDynamic(this, &UConfigMenuWidget::HandleResourceTabClicked);
	}
	if (StartButton) {
		StartButton->OnClicked.AddDynamic(this, &UConfigMenuWidget::HandleStartClicked);
	}
	if (LoadGameButton) {
		LoadGameButton->OnClicked.AddDynamic(this, &UConfigMenuWidget::HandleLoadGameClicked);
	}
	if (SaveConfigButton) {
		SaveConfigButton->OnClicked.AddDynamic(this, &UConfigMenuWidget::HandleSaveConfigClicked);
	}
	if (LoadConfigButton) {
		LoadConfigButton->OnClicked.AddDynamic(this, &UConfigMenuWidget::HandleLoadConfigClicked);
	}
	if (FailureMessage) {
		FailureMessage->SetVisibility(ESlateVisibility::Collapsed);
	}

	HandleStoryTabClicked();
}

void UConfigMenuWidget::ShowOnly(ETab Tab) {
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

void UConfigMenuWidget::HandleStoryTabClicked() {
	ShowOnly(ETab::Story);
	if (StoryPanel) {
		StoryPanel->RefreshRows();
	}
}

void UConfigMenuWidget::HandleModTabClicked() {
	ShowOnly(ETab::Mod);
	if (ModPanel) {
		ModPanel->RefreshDllList();
	}
}

void UConfigMenuWidget::HandleResourceTabClicked() {
	ShowOnly(ETab::Resource);
	if (ResourcePanel) {
		ResourcePanel->RefreshRows();
	}
}

void UConfigMenuWidget::HandleStartClicked() {
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

void UConfigMenuWidget::HandleSaveConfigClicked() {
	UGameInstance* gameInstance = GetGameInstance();
	UForeverConfigBridgeSubsystem* bridge = gameInstance ? gameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>() : nullptr;
	if (!bridge) {
		return;
	}

	FString error;
	if (!bridge->SaveConfigWithDialog(error) && !error.IsEmpty() && FailureMessage) {
		FailureMessage->SetText(FText::FromString(error));
		FailureMessage->SetVisibility(ESlateVisibility::Visible);
	}
}

void UConfigMenuWidget::HandleLoadConfigClicked() {
	UGameInstance* gameInstance = GetGameInstance();
	UForeverConfigBridgeSubsystem* bridge = gameInstance ? gameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>() : nullptr;
	if (!bridge) {
		return;
	}

	FString error;
	if (bridge->LoadConfigWithDialog(error)) {
		if (FailureMessage) {
			FailureMessage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else if (!error.IsEmpty() && FailureMessage) {
		FailureMessage->SetText(FText::FromString(error));
		FailureMessage->SetVisibility(ESlateVisibility::Visible);
	}

	// 成功时配置已换，失败时已恢复为config.json——两种情况都刷新，让界面与内存一致。
	if (StoryPanel) {
		StoryPanel->RefreshRows();
	}
	if (ModPanel) {
		ModPanel->RefreshDllList();
	}
	if (ResourcePanel) {
		ResourcePanel->RefreshRows();
	}
}

void UConfigMenuWidget::HandleLoadGameClicked() {
	UGameInstance* gameInstance = GetGameInstance();
	UForeverSaveSubsystem* saveSubsystem = gameInstance ? gameInstance->GetSubsystem<UForeverSaveSubsystem>() : nullptr;
	if (!saveSubsystem) {
		return;
	}

	// 成功时LoadWithDialog已经发起OpenLevel(World)；取消对话框时error为空，什么都不提示。
	FString error;
	if (!saveSubsystem->LoadWithDialog(error) && !error.IsEmpty()) {
		if (FailureMessage) {
			FailureMessage->SetText(FText::FromString(error));
			FailureMessage->SetVisibility(ESlateVisibility::Visible);
		}
	}
}
