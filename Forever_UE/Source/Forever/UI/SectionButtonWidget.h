#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SectionButtonWidget.generated.h"

class UButton;
class UTextBlock;
class AForeverFrameworkActor;

// 对应老蓝图SectionButton_C：USectionOptionWidget列表里的单个可点击分支选项行——和
// MeetButtonWidget（MeetOption列表的行）是两套独立的按钮类，字段/回调目标不同，不复用同一个
// 类。逻辑全部在C++基类里，Blueprint子类只需要摆一个命名Button的Button+命名Text的
// TextBlock，不写蓝图图表逻辑。
UCLASS()
class FOREVER_API USectionButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 由USectionOptionWidget::ShowOptions在创建每一行时调用一次。
	// @framework: 点击时转发给UForeverStoryFrameworkComponent::SelectDialogOption用
	// @index: 这一行对应当前分支Section::GetOptions()里的下标（点击后据此定位到具体Option）
	// @text: 已经求值好的选项文本（Option::GetOption()是DSL原文，求值在
	// UForeverStoryFrameworkComponent::ShowDialog里、Section还没离开原始ScriptContext时完成，
	// 见该函数声明处注释）
	void Setup(AForeverFrameworkActor* inFramework, int32 inIndex, const FString& text);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text;

private:
	TWeakObjectPtr<AForeverFrameworkActor> framework;
	int32 index = -1;
};
