#include "UI/SectionButtonWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverStoryFrameworkComponent.h"

void USectionButtonWidget::Setup(AForeverFrameworkActor* inFramework, int32 inIndex, const FString& text) {
	framework = inFramework;
	index = inIndex;

	if (Text) {
		Text->SetText(FText::FromString(text));
	}
}

void USectionButtonWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (Button) {
		Button->OnClicked.AddDynamic(this, &USectionButtonWidget::HandleClicked);
	}
}

void USectionButtonWidget::HandleClicked() {
	AForeverFrameworkActor* frameworkActor = framework.Get();
	if (!frameworkActor || !frameworkActor->GetStoryFramework()) return;

	frameworkActor->GetStoryFramework()->SelectDialogOption(index);
}
