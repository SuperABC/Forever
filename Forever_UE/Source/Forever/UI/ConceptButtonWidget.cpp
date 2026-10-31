#include "ConceptButtonWidget.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"

void UConceptButtonWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (SelectButton) {
		SelectButton->OnClicked.AddDynamic(this, &UConceptButtonWidget::HandleClicked);
	}
}

void UConceptButtonWidget::SetupButton(const FString& InConceptKey, TFunction<void(const FString&)> InOnClicked) {
	conceptKey = InConceptKey;
	onClicked = MoveTemp(InOnClicked);

	if (ConceptLabel) {
		ConceptLabel->SetText(FText::FromString(conceptKey));
	}
}

void UConceptButtonWidget::HandleClicked() {
	if (onClicked) {
		onClicked(conceptKey);
	}
}
