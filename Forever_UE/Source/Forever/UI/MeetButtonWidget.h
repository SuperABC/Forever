#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MeetButtonWidget.generated.h"

class UButton;
class UTextBlock;
class AForeverFrameworkActor;

// 对应老蓝图MeetButton_C：MeetOptionWidget管理的滚动列表里的一行，持有"这个选项属于哪个
// 对话目标(市民)/选项文本本身/是否全局选项"三个字段，点击时转发给
// UForeverStoryFrameworkComponent::OptionDialog。逻辑全部在C++基类里，Blueprint子类只需要
// 摆一个命名Button的Button + 命名Text的TextBlock，不写任何蓝图图表逻辑。
UCLASS()
class FOREVER_API UMeetButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 由UMeetOptionWidget::AddOption创建完实例后立刻调用一次，设置好这一行需要的全部数据并
	// 刷新显示文本。
	// @framework: 用于点击时转发到UForeverStoryFrameworkComponent::OptionDialog
	// @targetName: 对话目标（市民）姓名
	// @option: 选项文本
	// @bGlobal: 是否全局选项——这次只存字段，不实现对应的GlobalDialog匹配分支
	void Setup(AForeverFrameworkActor* inFramework, const FString& inTargetName, const FString& inOption, bool inGlobal);

	const FString& GetTargetName() const { return targetName; }
	const FString& GetOption() const { return option; }

	// 供UMeetOptionWidget::ClickFocus调用（键盘/手柄"确认"路径，不经过真实的鼠标点击）—
	// 对应老蓝图MeetButton::ClickButton，等价于HandleClicked，公开成一个独立方法而不是
	// 把HandleClicked本身开成public，跟"HandleClicked是Button.OnClicked的回调"这个身份
	// 分开。
	void ClickButton() { HandleClicked(); }

	// 供UMeetOptionWidget::NativeTick调用，切换这一行的高亮底色（当前Focus行=黄色，其余=
	// 默认白色）——对应老蓝图MeetOption的Tick逻辑，这次把"改颜色"这个实现细节封装在
	// MeetButton自己身上，MeetOptionWidget不需要知道Button这个子控件的存在。
	void SetRowHighlighted(bool bHighlighted);

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
	FString targetName;
	FString option;
	bool bGlobal = false;
};
