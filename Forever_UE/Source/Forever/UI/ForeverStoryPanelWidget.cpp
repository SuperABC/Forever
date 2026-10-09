#include "ForeverStoryPanelWidget.h"

#include "ForeverPathRowWidget.h"
#include "Mod/ForeverConfigBridgeSubsystem.h"

#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "ForeverScrollBoxUtils.h"

using ForeverScrollBoxUtils::AddChildWithGap;

void UForeverStoryPanelWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (AddScriptButton) {
		AddScriptButton->OnClicked.AddDynamic(this, &UForeverStoryPanelWidget::HandleAddScriptClicked);
	}

	RefreshRows();
}

void UForeverStoryPanelWidget::RefreshRows() {
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
		UForeverPathRowWidget* rowWidget = CreateWidget<UForeverPathRowWidget>(this, PathRowClass);
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

void UForeverStoryPanelWidget::HandleAddScriptClicked() {
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
