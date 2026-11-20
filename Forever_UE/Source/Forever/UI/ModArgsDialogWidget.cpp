#include "ModArgsDialogWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace {
	constexpr int32 kFontSize = 18;

	UTextBlock* MakeLabel(UWidgetTree* tree, const FString& text, int32 size = kFontSize) {
		UTextBlock* label = tree->ConstructWidget<UTextBlock>();
		FSlateFontInfo font = label->GetFont();
		font.Size = size;
		label->SetFont(font);
		label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		label->SetText(FText::FromString(text));
		return label;
	}

	UButton* MakeButton(UWidgetTree* tree, const FString& text) {
		UButton* button = tree->ConstructWidget<UButton>();
		button->SetContent(MakeLabel(tree, text));
		return button;
	}
}

void UModArgRowProxy::OnDeleteClicked() {
	if (UModArgsDialogWidget* owner = Owner.Get()) {
		owner->RemoveArgRow(Row.Get());
	}
}

void UModArgsDialogWidget::Open(const FString& InTitle, const FString& InArgs, TFunction<void(const FString&)> InOnConfirm) {
	onConfirm = MoveTemp(InOnConfirm);
	if (!WidgetTree || WidgetTree->RootWidget) return;

	// 根：全屏半透明遮罩(挡住下面的点击)，中间一块固定大小的面板。
	UBorder* dim = WidgetTree->ConstructWidget<UBorder>();
	dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	dim->SetHorizontalAlignment(HAlign_Center);
	dim->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = dim;

	USizeBox* size = WidgetTree->ConstructWidget<USizeBox>();
	size->SetWidthOverride(720.f);
	size->SetHeightOverride(560.f);
	dim->SetContent(size);

	UBorder* panel = WidgetTree->ConstructWidget<UBorder>();
	panel->SetBrushColor(FLinearColor(0.55f, 0.56f, 0.6f, 1.f));
	panel->SetPadding(FMargin(16.f));
	size->AddChild(panel);

	UVerticalBox* column = WidgetTree->ConstructWidget<UVerticalBox>();
	panel->SetContent(column);

	column->AddChildToVerticalBox(MakeLabel(WidgetTree, FString::Printf(TEXT("%s 的参数（一行一个）"), *InTitle), 22))->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

	RowList = WidgetTree->ConstructWidget<UScrollBox>();
	UVerticalBoxSlot* listSlot = column->AddChildToVerticalBox(RowList);
	listSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UHorizontalBox* buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	column->AddChildToVerticalBox(buttons)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));

	UButton* addButton = MakeButton(WidgetTree, TEXT("添加参数"));
	addButton->OnClicked.AddDynamic(this, &UModArgsDialogWidget::HandleAddClicked);
	buttons->AddChildToHorizontalBox(addButton);

	// 中间撑开，确定/取消靠右。
	UHorizontalBoxSlot* spacer = buttons->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USizeBox>());
	spacer->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UButton* cancelButton = MakeButton(WidgetTree, TEXT("取消"));
	cancelButton->OnClicked.AddDynamic(this, &UModArgsDialogWidget::HandleCancelClicked);
	buttons->AddChildToHorizontalBox(cancelButton)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

	UButton* okButton = MakeButton(WidgetTree, TEXT("确定"));
	okButton->OnClicked.AddDynamic(this, &UModArgsDialogWidget::HandleConfirmClicked);
	buttons->AddChildToHorizontalBox(okButton);

	// 按空白拆成token，每个一行。
	TArray<FString> tokens;
	InArgs.ParseIntoArrayWS(tokens);
	for (const FString& token : tokens) {
		AddArgRow(token);
	}
}

void UModArgsDialogWidget::AddArgRow(const FString& Text) {
	if (!WidgetTree || !RowList) return;

	UHorizontalBox* row = WidgetTree->ConstructWidget<UHorizontalBox>();

	UEditableTextBox* box = WidgetTree->ConstructWidget<UEditableTextBox>();
	// 浅色底+黑字，看得清。
	box->WidgetStyle.BackgroundColor = FSlateColor(FLinearColor::White);
	box->WidgetStyle.ForegroundColor = FSlateColor(FLinearColor::Black);
	box->WidgetStyle.FocusedForegroundColor = FSlateColor(FLinearColor::Black);
	box->SetText(FText::FromString(Text));
	row->AddChildToHorizontalBox(box)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UButton* del = MakeButton(WidgetTree, TEXT("删除"));
	UModArgRowProxy* proxy = NewObject<UModArgRowProxy>(this);
	proxy->Owner = this;
	proxy->Row = row;
	del->OnClicked.AddDynamic(proxy, &UModArgRowProxy::OnDeleteClicked);
	row->AddChildToHorizontalBox(del)->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));

	RowList->AddChild(row);
	Rows.Add(row);
	Boxes.Add(box);
	Proxies.Add(proxy);
}

void UModArgsDialogWidget::RemoveArgRow(UHorizontalBox* Row) {
	const int32 index = Rows.IndexOfByKey(Row);
	if (index == INDEX_NONE) return;
	Row->RemoveFromParent();
	Rows.RemoveAt(index);
	Boxes.RemoveAt(index);
	Proxies.RemoveAt(index);
}

FString UModArgsDialogWidget::JoinArgs() const {
	TArray<FString> parts;
	for (const UEditableTextBox* box : Boxes) {
		// 行内如果误输入了空格，当成多个参数(Core解析本来就按空白拆)；空行丢掉。
		TArray<FString> tokens;
		box->GetText().ToString().ParseIntoArrayWS(tokens);
		parts.Append(tokens);
	}
	return FString::Join(parts, TEXT(" "));
}

void UModArgsDialogWidget::HandleAddClicked() {
	AddArgRow(FString());
	if (!Boxes.IsEmpty()) Boxes.Last()->SetKeyboardFocus();
}

void UModArgsDialogWidget::HandleConfirmClicked() {
	if (onConfirm) onConfirm(JoinArgs());
	RemoveFromParent();
}

void UModArgsDialogWidget::HandleCancelClicked() {
	RemoveFromParent();
}
