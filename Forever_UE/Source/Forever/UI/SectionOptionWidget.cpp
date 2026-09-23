#include "UI/SectionOptionWidget.h"
#include "UI/SectionButtonWidget.h"

#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/PanelWidget.h"

// 选项行之间的竖直间距(UE单位)——和MeetOptionWidget::AddOption同一个约定/同一个数值，见
// 那边的说明。
namespace {
	constexpr float kOptionRowVerticalPadding = 10.f;
}

void USectionOptionWidget::ShowOptions(AForeverFrameworkActor* framework, const TArray<FString>& optionTexts) {
	if (!OptionContainer || !SectionButtonClass) return;

	OptionContainer->ClearChildren();
	for (int32 i = 0; i < optionTexts.Num(); i++) {
		USectionButtonWidget* button = CreateWidget<USectionButtonWidget>(this, SectionButtonClass);
		if (!button) continue;

		button->Setup(framework, i, optionTexts[i]);
		if (UScrollBoxSlot* slot = Cast<UScrollBoxSlot>(OptionContainer->AddChild(button))) {
			slot->SetPadding(FMargin(0.f, kOptionRowVerticalPadding, 0.f, kOptionRowVerticalPadding));
		}
	}

	SetVisibility(ESlateVisibility::Visible);
}

void USectionOptionWidget::HideOptions() {
	SetVisibility(ESlateVisibility::Collapsed);
}
