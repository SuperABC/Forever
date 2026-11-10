#include "StoryPanelWidget.h"

#include "PathRowWidget.h"
#include "ModCheckRowWidget.h"
#include "Mod/ForeverConfigBridgeSubsystem.h"

#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "ScrollBoxUtils.h"

using ScrollBoxUtils::AddChildWithGap;

void UStoryPanelWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (AddScriptButton) {
		AddScriptButton->OnClicked.AddDynamic(this, &UStoryPanelWidget::HandleAddScriptClicked);
	}

	RefreshRows();
}

void UStoryPanelWidget::RefreshRows() {
	RefreshMainStoryList();

	if (ScriptList) {
		ScriptList->ClearChildren();
	}

	if (!PathRowClass || !ScriptList) {
		return;
	}

	UGameInstance* gameInstance = GetGameInstance();
	UForeverConfigBridgeSubsystem* bridge = gameInstance ? gameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>() : nullptr;
	if (!bridge) {
		return;
	}

	for (const FString& path : bridge->GetStoryScripts()) {
		UPathRowWidget* rowWidget = CreateWidget<UPathRowWidget>(this, PathRowClass);
		if (!rowWidget) {
			continue;
		}

		rowWidget->SetupRow(path, [this](const FString& deletedPath) {
			UGameInstance* innerGameInstance = GetGameInstance();
			UForeverConfigBridgeSubsystem* innerBridge = innerGameInstance ? innerGameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>() : nullptr;
			if (innerBridge) {
				innerBridge->RemoveStoryScript(deletedPath);
			}
			RefreshRows();
		});
		AddChildWithGap(ScriptList, rowWidget);
	}
}

void UStoryPanelWidget::RefreshMainStoryList() {
	if (!MainStoryList) return;
	MainStoryList->ClearChildren();
	if (!ModCheckRowClass) return;

	UGameInstance* gameInstance = GetGameInstance();
	UForeverConfigBridgeSubsystem* bridge = gameInstance ? gameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>() : nullptr;
	if (!bridge) return;

	const FString selected = bridge->GetMainStory();
	for (const FString& modId : bridge->GetEnabledScriptMods()) {
		UModCheckRowWidget* row = CreateWidget<UModCheckRowWidget>(this, ModCheckRowClass);
		if (!row) continue;
		row->SetSelectRowData(modId, modId == selected, [this](const FString& picked) {
			UGameInstance* innerGameInstance = GetGameInstance();
			if (UForeverConfigBridgeSubsystem* innerBridge = innerGameInstance ? innerGameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>() : nullptr) {
				innerBridge->SetMainStory(picked);
			}
			RefreshMainStoryList();
		});
		AddChildWithGap(MainStoryList, row);
	}
}

void UStoryPanelWidget::HandleAddScriptClicked() {
	UGameInstance* gameInstance = GetGameInstance();
	UForeverConfigBridgeSubsystem* bridge = gameInstance ? gameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>() : nullptr;
	if (!bridge) {
		return;
	}

	FString selected;
	if (bridge->SelectFile(TEXT("Script Files (*.script)|*.script"), selected)) {
		bridge->AddStoryScript(selected);
		RefreshRows();
	}
}
