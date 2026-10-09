#include "ForeverPathRowWidget.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"

void UForeverPathRowWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (DeleteButton) {
		DeleteButton->OnClicked.AddDynamic(this, &UForeverPathRowWidget::HandleDeleteClicked);
	}
}

void UForeverPathRowWidget::SetupRow(const FString& InPath, TFunction<void(const FString&)> InOnDelete) {
	path = InPath;
	onDelete = MoveTemp(InOnDelete);

	if (PathLabel) {
		PathLabel->SetText(FText::FromString(path));
	}
}

void UForeverPathRowWidget::HandleDeleteClicked() {
	if (onDelete) {
		onDelete(path);
	}
}
