#include "UI/MeetButtonWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverStoryFrameworkComponent.h"

void UMeetButtonWidget::Setup(AForeverFrameworkActor* inFramework, const FString& inTargetName,
	const FString& inOption) {
	framework = inFramework;
	targetName = inTargetName;
	option = inOption;

	if (Text) {
		Text->SetText(FText::FromString(option));
	}
}

void UMeetButtonWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (Button) {
		Button->OnClicked.AddDynamic(this, &UMeetButtonWidget::HandleClicked);
	}
}

void UMeetButtonWidget::SetRowHighlighted(bool bHighlighted) {
	if (Button) {
		Button->SetBackgroundColor(bHighlighted ? FLinearColor::Yellow : FLinearColor::White);
	}
}

void UMeetButtonWidget::HandleClicked() {
	AForeverFrameworkActor* frameworkActor = framework.Get();
	if (!frameworkActor || !frameworkActor->GetStoryFramework()) return;

	frameworkActor->GetStoryFramework()->OptionDialog(targetName, option);
}
