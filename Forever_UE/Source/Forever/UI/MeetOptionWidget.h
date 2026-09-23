#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MeetOptionWidget.generated.h"

class UScrollBox;
class UMeetButtonWidget;
class AForeverFrameworkActor;

// 对应老蓝图MeetOption_C：靠近一个市民时弹出的对话选项列表。滚动列表本体是OptionContainer
// （ScrollBox），每个选项是一个UMeetButtonWidget子控件；Focus记录当前键盘/滚轮高亮的行下标，
// OptionMap按"目标姓名+下标"这个复合key记录每个选项对应的控件，供RemoveName批量移除。
// 逻辑全部在C++基类里，Blueprint子类只需要摆一个命名OptionContainer的ScrollBox，
// MeetButtonClass指向对应的MeetButton Blueprint，不写任何蓝图图表逻辑。
UCLASS()
class FOREVER_API UMeetOptionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 新增一个选项行——由ACitizenElement::OnOverlapBegin在玩家靠近市民时，对
	// citizen->GetOptions()里的每一项调用一次。
	// @framework: 转发给新建的MeetButton用于点击回调
	// @option: 选项文本
	// @name: 对话目标（市民）姓名
	// @idx: 这条选项在目标options列表里的下标，和name一起构成OptionMap的复合key
	// @bGlobal: 是否全局选项，透传给MeetButton
	void AddOption(AForeverFrameworkActor* framework, const FString& option, const FString& name, int32 idx, bool bGlobal);

	// 移除属于某个目标（市民）的全部选项——由ACitizenElement::OnOverlapEnd在玩家离开市民
	// 检测范围时调用。
	void RemoveName(const FString& name);

	// 键盘/滚轮上下移动高亮行，空列表或已在边界时no-op。
	void FocusUp();
	void FocusDown();

	// 确认选中当前高亮行——对应老蓝图MeetOption::ClickFocus，转发给对应MeetButton的
	// ClickButton()。
	void ClickFocus();

protected:
	// 每帧刷新当前Focus行的高亮颜色（黄色），其它行恢复默认（白色），并把高亮行滚动进可视
	// 区域——对应老蓝图MeetOption的Tick逻辑。只在Focus实际变化时才重刷颜色，避免每帧无谓
	// SetBackgroundColor。
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> OptionContainer;

	UPROPERTY(EditDefaultsOnly, Category = "MeetOption")
	TSubclassOf<UMeetButtonWidget> MeetButtonClass;

private:
	static FString MakeKey(const FString& name, int32 idx);

	int32 Focus = 0;
	int32 lastHighlighted = -1;

	UPROPERTY()
	TMap<FString, TObjectPtr<UMeetButtonWidget>> OptionMap;
};
