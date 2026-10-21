#include "UI/ForeverIdentifyTagWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

bool UForeverIdentifyTagWidget::Initialize() {
	bool ok = Super::Initialize();
	if (!ok || !WidgetTree || WidgetTree->RootWidget) return ok; // 已经有控件树(换成了WBP)就不再搭

	UCanvasPanel* root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = root;

	// 竖排容器铺满整个控件，行靠下排列(见SetLines)。
	linesBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Lines"));
	if (UCanvasPanelSlot* slot = root->AddChildToCanvas(linesBox)) {
		slot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		slot->SetOffsets(FMargin(0.f));
	}
	return ok;
}

void UForeverIdentifyTagWidget::SetLines(const TArray<FString>& lines, float resolutionScale) {
	if (!linesBox || !WidgetTree) return;
	linesBox->ClearChildren();

	// 顶部放一个填满剩余空间的占位，把所有行挤到控件底部。
	USpacer* filler = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	if (UVerticalBoxSlot* slot = Cast<UVerticalBoxSlot>(linesBox->AddChild(filler))) {
		slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	for (const FString& line : lines) {
		UTextBlock* text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		text->SetText(FText::FromString(line));
		text->SetJustification(ETextJustify::Center);
		text->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		FSlateFontInfo font = text->GetFont();
		font.Size = FMath::RoundToInt(fontSize * FMath::Max(resolutionScale, 0.1f));
		font.OutlineSettings.OutlineSize = outlineSize;
		font.OutlineSettings.OutlineColor = FLinearColor::Black;
		text->SetFont(font);

		if (UVerticalBoxSlot* slot = Cast<UVerticalBoxSlot>(linesBox->AddChild(text))) {
			slot->SetHorizontalAlignment(HAlign_Center);
			slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}
}
