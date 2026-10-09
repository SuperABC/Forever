#include "ForeverModCheckRowWidget.h"

#include "Mod/ForeverConfigBridgeSubsystem.h"

#include "Components/TextBlock.h"
#include "Components/CheckBox.h"

void UForeverModCheckRowWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (EnableCheckBox) {
		EnableCheckBox->OnCheckStateChanged.AddDynamic(this, &UForeverModCheckRowWidget::HandleCheckChanged);
	}
}

void UForeverModCheckRowWidget::SetRowData(const FString& InDllPath, const FString& InConceptKey,
	const FString& InModId, bool bEnabled, bool bLocked, TFunction<void()> InOnChanged) {
	dllPath = InDllPath;
	conceptKey = InConceptKey;
	modId = InModId;
	onChanged = MoveTemp(InOnChanged);

	if (ModIdLabel) {
		ModIdLabel->SetText(FText::FromString(modId));
	}
	if (EnableCheckBox) {
		EnableCheckBox->SetIsChecked(bLocked ? true : bEnabled);
		EnableCheckBox->SetIsEnabled(!bLocked);
	}
	SetIsEnabled(!bLocked);
}

void UForeverModCheckRowWidget::HandleCheckChanged(bool bIsChecked) {
	if (UGameInstance* gameInstance = GetGameInstance()) {
		if (UForeverConfigBridgeSubsystem* bridge = gameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>()) {
			bridge->SetModEnabled(conceptKey, modId, bIsChecked);
		}
	}
	if (onChanged) {
		onChanged();
	}
}
