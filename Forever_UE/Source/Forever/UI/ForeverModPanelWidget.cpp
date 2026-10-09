#include "ForeverModPanelWidget.h"

#include "ForeverPathRowWidget.h"
#include "ForeverConceptButtonWidget.h"
#include "ForeverModCheckRowWidget.h"
#include "Mod/ForeverConfigBridgeSubsystem.h"

#include "common/loader.h"

#include "Components/Button.h"
#include "ForeverScrollBoxUtils.h"

using ForeverScrollBoxUtils::AddChildWithGap;

void UForeverModPanelWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (AddDllPathButton) {
		AddDllPathButton->OnClicked.AddDynamic(this, &UForeverModPanelWidget::HandleAddDllPathClicked);
	}

	// 中列的21个concept按钮按GetModConceptDescriptors()的固定顺序生成一次——这张表的内容
	// 运行期不会变，不需要跟着RefreshDllList/RefreshModList重新生成。
	if (ConceptList && ConceptButtonClass) {
		ConceptList->ClearChildren();
		for (const ModConceptDescriptor& descriptor : GetModConceptDescriptors()) {
			UForeverConceptButtonWidget* button = CreateWidget<UForeverConceptButtonWidget>(this, ConceptButtonClass);
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

void UForeverModPanelWidget::SelectConcept(const FString& ConceptKey) {
	selectedConceptKey = ConceptKey;
	RefreshModList();
}

void UForeverModPanelWidget::RefreshModList() {
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

	for (const FForeverModRow& row : bridge->GetModRows()) {
		if (row.ConceptKey != selectedConceptKey) {
			continue;
		}

		UForeverModCheckRowWidget* rowWidget = CreateWidget<UForeverModCheckRowWidget>(this, ModCheckRowClass);
		if (!rowWidget) {
			continue;
		}

		rowWidget->SetRowData(row.DllPath, row.ConceptKey, row.ModId, row.bEnabled, row.bLocked,
			[this]() { RefreshModList(); });
		AddChildWithGap(ModList, rowWidget);
	}
}

void UForeverModPanelWidget::RefreshDllList() {
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
		UForeverPathRowWidget* rowWidget = CreateWidget<UForeverPathRowWidget>(this, PathRowClass);
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

void UForeverModPanelWidget::HandleAddDllPathClicked() {
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
