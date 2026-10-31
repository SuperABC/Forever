#include "ModPanelWidget.h"

#include "PathRowWidget.h"
#include "ConceptButtonWidget.h"
#include "ModCheckRowWidget.h"
#include "Mod/ForeverConfigBridgeSubsystem.h"

#include "common/loader.h"

#include "Components/Button.h"
#include "ScrollBoxUtils.h"

using ScrollBoxUtils::AddChildWithGap;

void UModPanelWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (AddDllPathButton) {
		AddDllPathButton->OnClicked.AddDynamic(this, &UModPanelWidget::HandleAddDllPathClicked);
	}

	// 中列的21个concept按钮按GetModConceptDescriptors()的固定顺序生成一次——这张表的内容
	// 运行期不会变，不需要跟着RefreshDllList/RefreshModList重新生成。
	if (ConceptList && ConceptButtonClass) {
		ConceptList->ClearChildren();
		for (const ModConceptDescriptor& descriptor : GetModConceptDescriptors()) {
			UConceptButtonWidget* button = CreateWidget<UConceptButtonWidget>(this, ConceptButtonClass);
			if (!button) {
				continue;
			}
			FString conceptKey = UTF8_TO_TCHAR(descriptor.conceptKey);
			button->SetupButton(conceptKey, [this](const FString& clickedKey) { SelectConcept(clickedKey); });
			AddChildWithGap(ConceptList, button);
		}
	}

	RefreshDllList();
}

void UModPanelWidget::SelectConcept(const FString& ConceptKey) {
	selectedConceptKey = ConceptKey;
	RefreshModList();
}

void UModPanelWidget::RefreshModList() {
	if (ModList) {
		ModList->ClearChildren();
	}

	if (!ModList || !ModCheckRowClass || selectedConceptKey.IsEmpty()) {
		return;
	}

	UGameInstance* gameInstance = GetGameInstance();
	UForeverConfigBridgeSubsystem* bridge = gameInstance ? gameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>() : nullptr;
	if (!bridge) {
		return;
	}

	for (const FModRow& row : bridge->GetModRows()) {
		if (row.ConceptKey != selectedConceptKey) {
			continue;
		}

		UModCheckRowWidget* rowWidget = CreateWidget<UModCheckRowWidget>(this, ModCheckRowClass);
		if (!rowWidget) {
			continue;
		}

		rowWidget->SetRowData(row.DllPath, row.ConceptKey, row.ModId, row.bEnabled, row.bLocked, row.Dependents,
			[this]() { RefreshModList(); });
		AddChildWithGap(ModList, rowWidget);
	}
}

void UModPanelWidget::RefreshDllList() {
	if (DllList) {
		DllList->ClearChildren();
	}

	if (!DllList || !PathRowClass) {
		return;
	}

	UGameInstance* gameInstance = GetGameInstance();
	UForeverConfigBridgeSubsystem* bridge = gameInstance ? gameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>() : nullptr;
	if (!bridge) {
		return;
	}

	for (const FString& dllPath : bridge->GetDllPaths()) {
		UPathRowWidget* rowWidget = CreateWidget<UPathRowWidget>(this, PathRowClass);
		if (!rowWidget) {
			continue;
		}

		rowWidget->SetupRow(dllPath, [this](const FString& deletedPath) {
			UGameInstance* innerGameInstance = GetGameInstance();
			UForeverConfigBridgeSubsystem* innerBridge = innerGameInstance ? innerGameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>() : nullptr;
			if (innerBridge) {
				innerBridge->RemoveDllPath(deletedPath);
			}
			RefreshDllList();
			RefreshModList();
		});
		AddChildWithGap(DllList, rowWidget);
	}
}

void UModPanelWidget::HandleAddDllPathClicked() {
	UGameInstance* gameInstance = GetGameInstance();
	UForeverConfigBridgeSubsystem* bridge = gameInstance ? gameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>() : nullptr;
	if (!bridge) {
		return;
	}

	FString selected;
	if (bridge->SelectFolder(selected)) {
		bridge->AddDllPath(selected);
		RefreshDllList();
		RefreshModList();
	}
}
