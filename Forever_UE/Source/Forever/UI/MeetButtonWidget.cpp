#include "UI/MeetButtonWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverStoryFrameworkComponent.h"

void UMeetButtonWidget::Setup(AForeverFrameworkActor* inFramework, const FString& inTargetName,
	const FString& inOption, bool inGlobal) {
	framework = inFramework;
	targetName = inTargetName;
	option = inOption;
	bGlobal = inGlobal;

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

	// bGlobal这次只存字段，不实现对应的GlobalDialog（只匹配主线剧情的全局选项分支）——
	// 见UForeverStoryFrameworkComponent::OptionDialog声明处注释，范围明确不做。
	frameworkActor->GetStoryFramework()->OptionDialog(targetName, option);
}
