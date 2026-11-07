#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SectionOptionWidget.generated.h"

class UScrollBox;
class USectionButtonWidget;
class AForeverFrameworkActor;

// 剧情对话走到分支选项段(Section::IsBranch())时一次性弹出的选项列表；与MeetOptionWidget是两套独立UI
// (后者是靠近市民能问什么)。
UCLASS()
class FOREVER_API USectionOptionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 显示一组分支选项（文本已经在UForeverStoryFrameworkComponent::ShowDialog里求值好），
	// 并把自己设为可见——由UForeverStoryFrameworkComponent::ShowCurrentItem在轮到一个分支
	// Section时调用。
	// @framework: 转发给每个新建SectionButton，点击时用于回调SelectDialogOption
	// @optionTexts: 已求值的选项文本列表，下标就是SelectDialogOption的index参数
	void ShowOptions(AForeverFrameworkActor* framework, const TArray<FString>& optionTexts);

	// 隐藏自己——轮到下一项（普通台词或没有更多内容）时调用。
	void HideOptions();

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> OptionContainer;

	UPROPERTY(EditDefaultsOnly, Category = "SectionOption")
	TSubclassOf<USectionButtonWidget> SectionButtonClass;
};
