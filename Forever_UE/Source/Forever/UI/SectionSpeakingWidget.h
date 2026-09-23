#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SectionSpeakingWidget.generated.h"

class UButton;
class UTextBlock;
class AForeverFrameworkActor;

// 对应老蓝图SectionSpeaking_C：剧情对话里一条普通台词（发言者+内容）的展示框，玩家点击
// ClickCatcher（铺满整个控件的按钮，见.dump/SectionSpeaking.txt）或按空格键关闭当前这一条、
// 推进到下一条——对应UForeverStoryFrameworkComponent::AdvanceDialog。老蓝图还有一段"至少
// 展示skip_dialog_duration秒才允许点掉"的防误触逻辑（记录Construct时的GetRealTimeSeconds，
// 点击时跟这个阈值比较），这次简化掉不实现——不是这次"用SectionSpeaking/SectionOption两个
// UI展示对话"要求的核心行为，之后需要再单独加。逻辑全部在C++基类里，Blueprint子类只需要摆
// Speaker/Content两个TextBlock+一个铺满整个控件的ClickCatcher按钮，不写蓝图图表逻辑。
//
// 空格键走原生键盘事件（NativeOnKeyDown）而不是Enhanced Input：对话展示期间
// UForeverStoryFrameworkComponent::ShowCurrentItem把输入模式切成UIOnly（仿照老工程
// SectionButton点击后调SetInputMode_GameOnly、隐含之前是"游戏动作不响应"这个状态，
// 见.dump/SelectionButton.txt），UIOnly下Pawn的Enhanced Input完全收不到输入（这正是它的
// 意义所在——不用像"空格键同时绑了Jump和推进对话"那样一个个手动判断"对话是不是在展示"来
// 避免误触，UIOnly从根上就不会把输入路由给Pawn），所以空格键只能在真正拿到键盘焦点的这个
// Widget自己身上接（ShowCurrentItem用SetInputMode_UIOnlyEx时把WidgetToFocus指定成这个
// Widget，见该函数实现），不能再走AForeverCharacter的按键绑定。
UCLASS()
class FOREVER_API USectionSpeakingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 显示一条台词并把自己设为可见——由UForeverStoryFrameworkComponent::ShowCurrentItem在
	// 轮到一个普通(非分支)Section时调用。speaker为空时隐藏发言者文本行（对应老蓝图test.json
	// 里speaker为空字符串、只显示content的用法，见ForeverStoryFrameworkComponent.md）。
	// @framework: 点击ClickCatcher/按空格时转发给UForeverStoryFrameworkComponent::AdvanceDialog用
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
