#include "UI/BackgroundSpeakingWidget.h"

#include "Components/TextBlock.h"

void UBackgroundSpeakingWidget::NativeConstruct() {
	Super::NativeConstruct();
	SetIsFocusable(false);
}

void UBackgroundSpeakingWidget::ShowLine(const FString& speaker, const FString& content) {
	if (Speaker) {
		Speaker->SetText(FText::FromString(speaker));
		Speaker->SetVisibility(speaker.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Content) {
		Content->SetText(FText::FromString(content));
	}
	// HitTestInvisible：看得见但不接收鼠标，后台对话不能挡住玩家点场景/MeetOption。
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UBackgroundSpeakingWidget::HideLine() {
	SetVisibility(ESlateVisibility::Collapsed);
}
