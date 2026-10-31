#include "PathRowWidget.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"

void UPathRowWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (DeleteButton) {
		DeleteButton->OnClicked.AddDynamic(this, &UPathRowWidget::HandleDeleteClicked);
	}
}

void UPathRowWidget::SetupRow(const FString& InPath, TFunction<void(const FString&)> InOnDelete) {
	path = InPath;
	onDelete = MoveTemp(InOnDelete);

	if (PathLabel) {
		PathLabel->SetText(FText::FromString(path));
	}
}

void UPathRowWidget::HandleDeleteClicked() {
	if (onDelete) {
		onDelete(path);
	}
}
