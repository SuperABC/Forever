#include "ModCheckRowWidget.h"

#include "Mod/ForeverConfigBridgeSubsystem.h"

#include "Components/TextBlock.h"
#include "Components/CheckBox.h"
#include "Components/Border.h"

namespace {
	// 引擎默认ToolTip字号是8(CoreStyle "ToolTip.Font")，太小，放大到20。
	constexpr int32 kModRowToolTipFontSize = 20;
}

void UModCheckRowWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (EnableCheckBox) {
		EnableCheckBox->OnCheckStateChanged.AddDynamic(this, &UModCheckRowWidget::HandleCheckChanged);
	}
}

void UModCheckRowWidget::SetRowData(const FString& InDllPath, const FString& InConceptKey,
	const FString& InModId, bool bEnabled, bool bLocked, const TArray<FString>& InDependents, TFunction<void()> InOnChanged) {
	dllPath = InDllPath;
	conceptKey = InConceptKey;
	modId = InModId;
	onChanged = MoveTemp(InOnChanged);

	// 被依赖(bLocked)有两种状态：已勾选=整行变灰、勾选框锁死不能取消；还没勾选=整行文字标红提醒必须勾上，
	// 勾选框保持未勾选且可点，勾上之后刷新成前一种状态。
	const bool bRequiredMissing = bLocked && !bEnabled;
	const bool bLockedOn = bLocked && bEnabled;

	if (ModIdLabel) {
		ModIdLabel->SetText(FText::FromString(modId));
		if (!bDefaultLabelColorCaptured) {
			defaultLabelColor = ModIdLabel->GetColorAndOpacity();
			bDefaultLabelColorCaptured = true;
		}
		ModIdLabel->SetColorAndOpacity(bRequiredMissing ? FSlateColor(FLinearColor::Red) : defaultLabelColor);
	}
	if (EnableCheckBox) {
		EnableCheckBox->SetIsChecked(bEnabled);
		EnableCheckBox->SetIsEnabled(!bLockedOn);
	}
	SetIsEnabled(!bLockedOn);

	// 悬停提示：Slate对禁用(变灰)的控件照样显示ToolTip。没被依赖时不设提示。
	UWidget* tooltip = nullptr;
	if (bLocked && InDependents.Num() > 0) {
		tooltip = BuildDependentsToolTip(TEXT("被以下内容依赖：\n") + FString::Join(InDependents, TEXT("\n")));
	}
	SetToolTip(tooltip);
	if (EnableCheckBox) {
		EnableCheckBox->SetToolTip(tooltip);
	}
}

UWidget* UModCheckRowWidget::BuildDependentsToolTip(const FString& Text) {
	// 自定义ToolTip控件(而不是SetToolTipText)才能改字号。UMG把自定义ToolTip包进一个没有背景的
	// SToolTip，所以背景和内边距要自己用UBorder补上。
	UBorder* border = NewObject<UBorder>(this);
	border->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.95f));
	border->SetPadding(FMargin(10.f, 6.f));

	UTextBlock* label = NewObject<UTextBlock>(this);
	FSlateFontInfo font = label->GetFont();
	font.Size = kModRowToolTipFontSize;
	label->SetFont(font);
	label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	label->SetText(FText::FromString(Text));
	border->SetContent(label);
	return border;
}

void UModCheckRowWidget::HandleCheckChanged(bool bIsChecked) {
	if (UGameInstance* gameInstance = GetGameInstance()) {
		if (UForeverConfigBridgeSubsystem* bridge = gameInstance->GetSubsystem<UForeverConfigBridgeSubsystem>()) {
			bridge->SetModEnabled(conceptKey, modId, bIsChecked);
		}
	}
	if (onChanged) {
		onChanged();
	}
}
