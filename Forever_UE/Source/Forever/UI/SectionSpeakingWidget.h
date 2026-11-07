#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SectionSpeakingWidget.generated.h"

class UButton;
class UTextBlock;
class AForeverFrameworkActor;

// 剧情对话里一条普通台词（发言者+内容）的展示框，点击ClickCatcher或按空格推进到下一条
// (UForeverStoryFrameworkComponent::AdvanceDialog)。空格走NativeOnKeyDown而不是Enhanced Input：
// 展示期间输入模式是UIOnly，Pawn收不到输入，键只能在拿到键盘焦点的本Widget上接。见SectionSpeakingWidget.md。
UCLASS()
class FOREVER_API USectionSpeakingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 显示一条台词并设为可见，由UForeverStoryFrameworkComponent::ShowCurrentItem在轮到普通Section时调用。
	// speaker为空时隐藏发言者文本行。framework用于点击/空格时转发AdvanceDialog。
	void ShowLine(AForeverFrameworkActor* framework, const FString& speaker, const FString& content);

	// 隐藏自己——轮到下一项（分支选项或没有更多内容）时调用。
	void HideLine();

protected:
	virtual void NativeConstruct() override;

	// 空格键推进——见类注释里"为什么走原生键盘事件而不是Enhanced Input"的说明。只处理
	// SpaceBar，其它键交给基类默认处理。
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void HandleClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ClickCatcher;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Speaker;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Content;

private:
	TWeakObjectPtr<AForeverFrameworkActor> framework;
};
