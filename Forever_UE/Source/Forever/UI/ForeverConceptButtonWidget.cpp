#include "ForeverConceptButtonWidget.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"

void UForeverConceptButtonWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (SelectButton) {
		SelectButton->OnClicked.AddDynamic(this, &UForeverConceptButtonWidget::HandleClicked);
	}
}

void UForeverConceptButtonWidget::SetupButton(const FString& InConceptKey, TFunction<void(const FString&)> InOnClicked) {
	conceptKey = InConceptKey;
	onClicked = MoveTemp(InOnClicked);

	if (ConceptLabel) {
		ConceptLabel->SetText(FText::FromString(conceptKey));
	}
}

void UForeverConceptButtonWidget::HandleClicked() {
	if (onClicked) {
		onClicked(conceptKey);
	}
}
