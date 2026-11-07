#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MeetButtonWidget.generated.h"

class UButton;
class UTextBlock;
class AForeverFrameworkActor;

// MeetOptionWidget列表里的一行，持有对话目标(市民)和选项文本，点击时转发给
// UForeverStoryFrameworkComponent::OptionDialog。Blueprint子类只需要摆命名Button和命名Text。
UCLASS()
class FOREVER_API UMeetButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 由UMeetOptionWidget::AddOption创建后立刻调用一次，设置数据并刷新显示文本。
	// @framework: 用于点击时转发到UForeverStoryFrameworkComponent::OptionDialog
	// @targetName: 对话目标（市民）姓名
	// @option: 选项文本
	void Setup(AForeverFrameworkActor* inFramework, const FString& inTargetName, const FString& inOption);

	const FString& GetTargetName() const { return targetName; }
	const FString& GetOption() const { return option; }

	// 供UMeetOptionWidget::ClickFocus调用(键盘确认路径，不经过鼠标点击)，等价于HandleClicked。
	void ClickButton() { HandleClicked(); }

	// 供UMeetOptionWidget::NativeTick调用，切换高亮底色(Focus行黄色，其余默认白色)。
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
};
