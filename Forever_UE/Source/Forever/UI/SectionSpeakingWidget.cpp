#include "UI/SectionSpeakingWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverStoryFrameworkComponent.h"

void USectionSpeakingWidget::ShowLine(AForeverFrameworkActor* inFramework, const FString& speaker, const FString& content) {
	framework = inFramework;

	if (Speaker) {
		Speaker->SetText(FText::FromString(speaker));
		Speaker->SetVisibility(speaker.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (Content) {
		Content->SetText(FText::FromString(content));
	}

	SetVisibility(ESlateVisibility::Visible);
}

void USectionSpeakingWidget::HideLine() {
	SetVisibility(ESlateVisibility::Collapsed);
}

FReply USectionSpeakingWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) {
	if (InKeyEvent.GetKey() == EKeys::SpaceBar) {
		HandleClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USectionSpeakingWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (ClickCatcher) {
		ClickCatcher->OnClicked.AddDynamic(this, &USectionSpeakingWidget::HandleClicked);
	}
}

void USectionSpeakingWidget::HandleClicked() {
	AForeverFrameworkActor* frameworkActor = framework.Get();
	if (!frameworkActor || !frameworkActor->GetStoryFramework()) return;

	frameworkActor->GetStoryFramework()->AdvanceDialog();
}
