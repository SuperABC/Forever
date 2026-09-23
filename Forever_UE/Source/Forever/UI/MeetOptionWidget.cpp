#include "UI/MeetOptionWidget.h"
#include "UI/MeetButtonWidget.h"

#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/PanelWidget.h"

// 选项行之间的竖直间距(UE单位)——AddOption时施加在每个新增MeetButton的ScrollBoxSlot上，
// 老蓝图MeetOption::AddOption同样给每个新按钮的Padding设了Top/Bottom各10。
namespace {
	constexpr float kOptionRowVerticalPadding = 10.f;
}

FString UMeetOptionWidget::MakeKey(const FString& name, int32 idx) {
	return FString::Printf(TEXT("%s#%d"), *name, idx);
}

void UMeetOptionWidget::AddOption(AForeverFrameworkActor* framework, const FString& option, const FString& name,
	int32 idx, bool bGlobal) {
	if (!OptionContainer || !MeetButtonClass) return;

	UMeetButtonWidget* button = CreateWidget<UMeetButtonWidget>(this, MeetButtonClass);
	if (!button) return;

	button->Setup(framework, name, option, bGlobal);
	if (UScrollBoxSlot* slot = Cast<UScrollBoxSlot>(OptionContainer->AddChild(button))) {
		slot->SetPadding(FMargin(0.f, kOptionRowVerticalPadding, 0.f, kOptionRowVerticalPadding));
	}
	OptionMap.Add(MakeKey(name, idx), button);

	// 新增一行之后强制下一次NativeTick重新刷一遍高亮——lastHighlighted缓存的是"上一次已经
	// 上色过的Focus值"，如果不失效，新加进来的按钮在Focus恰好等于lastHighlighted时(比如
	// 刚靠近一个新市民、Focus还停留在0，而上一个市民也是在Focus=0时离开的)不会被立刻上色，
	// 表现为"刚靠近选项没有高亮，要滚一下轮子才开始高亮"（滚轮改变了Focus，才触发刷新）。
	lastHighlighted = -1;
}

void UMeetOptionWidget::RemoveName(const FString& name) {
	if (!OptionContainer) return;

	for (auto it = OptionMap.CreateIterator(); it; ++it) {
		UMeetButtonWidget* button = it->Value;
		if (!button || button->GetTargetName() == name) {
			if (button) OptionContainer->RemoveChild(button);
			it.RemoveCurrent();
		}
	}

	Focus = FMath::Clamp(Focus, 0, FMath::Max(OptionContainer->GetChildrenCount() - 1, 0));
	// 同AddOption：列表构成变了，强制下一次NativeTick重新判定谁该高亮，见AddOption里的说明。
	lastHighlighted = -1;
}

void UMeetOptionWidget::FocusUp() {
	if (!OptionContainer || OptionContainer->GetChildrenCount() <= 0) return;
	Focus = FMath::Max(Focus - 1, 0);
}

void UMeetOptionWidget::FocusDown() {
	if (!OptionContainer || OptionContainer->GetChildrenCount() <= 0) return;
	Focus = FMath::Min(Focus + 1, OptionContainer->GetChildrenCount() - 1);
}

void UMeetOptionWidget::ClickFocus() {
	if (!OptionContainer || OptionContainer->GetChildrenCount() <= Focus) return;

	if (UMeetButtonWidget* button = Cast<UMeetButtonWidget>(OptionContainer->GetChildAt(Focus))) {
		button->ClickButton();
	}
}

void UMeetOptionWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime) {
	Super::NativeTick(MyGeometry, DeltaTime);

	if (!OptionContainer) return;

	int32 childCount = OptionContainer->GetChildrenCount();
	Focus = FMath::Clamp(Focus, 0, FMath::Max(childCount - 1, 0));

	if (Focus == lastHighlighted) return;
	lastHighlighted = Focus;

	for (int32 i = 0; i < childCount; i++) {
		if (UMeetButtonWidget* button = Cast<UMeetButtonWidget>(OptionContainer->GetChildAt(i))) {
			button->SetRowHighlighted(i == Focus);
		}
	}

	if (childCount > Focus) {
		if (UMeetButtonWidget* focused = Cast<UMeetButtonWidget>(OptionContainer->GetChildAt(Focus))) {
			OptionContainer->ScrollWidgetIntoView(focused);
		}
	}
}
