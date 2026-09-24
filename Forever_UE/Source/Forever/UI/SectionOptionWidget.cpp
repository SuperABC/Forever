#include "UI/SectionOptionWidget.h"
#include "UI/SectionButtonWidget.h"

#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/PanelWidget.h"

// 选项行之间的竖直间距(UE单位)——和MeetOptionWidget::AddOption同一个约定/同一个数值，见
// 那边的说明。名字加SectionOption前缀避免和MeetOptionWidget.cpp里同名的匿名namespace常量
// 在unity build下撞名，见那边的说明。
namespace {
	constexpr float kSectionOptionRowVerticalPadding = 10.f;
}

void USectionOptionWidget::ShowOptions(AForeverFrameworkActor* framework, const TArray<FString>& optionTexts) {
	if (!OptionContainer || !SectionButtonClass) return;

	OptionContainer->ClearChildren();
	for (int32 i = 0; i < optionTexts.Num(); i++) {
		USectionButtonWidget* button = CreateWidget<USectionButtonWidget>(this, SectionButtonClass);
		if (!button) continue;

		button->Setup(framework, i, optionTexts[i]);
		if (UScrollBoxSlot* slot = Cast<UScrollBoxSlot>(OptionContainer->AddChild(button))) {
			slot->SetPadding(FMargin(0.f, kSectionOptionRowVerticalPadding, 0.f, kSectionOptionRowVerticalPadding));
		}
	}

	SetVisibility(ESlateVisibility::Visible);
}

void USectionOptionWidget::HideOptions() {
	SetVisibility(ESlateVisibility::Collapsed);
}
