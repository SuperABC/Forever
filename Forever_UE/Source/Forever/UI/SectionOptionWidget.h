#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SectionOptionWidget.generated.h"

class UScrollBox;
class USectionButtonWidget;
class AForeverFrameworkActor;

// 对应老蓝图SectionOption_C：剧情对话遇到分支选项段（Section::IsBranch()为true）时弹出的
// 选项列表——和MeetOptionWidget是两套独立UI，管的是不同的东西：MeetOption管"靠近市民能问
// 什么"（AddOptionChange攒起来的选项，随时可能有多个，靠滚轮/F操作），这个管"剧情对话本身
// 走到分支时该怎么选"（一次性弹出、选完就消失，见.dump/SectionOption.txt）。逻辑全部在C++
// 基类里，Blueprint子类只需要摆一个命名OptionContainer的ScrollBox，SectionButtonClass指向
// 对应的SectionButton Blueprint，不写任何蓝图图表逻辑。
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
