#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForeverThrowListWidget.generated.h"

class UScrollBox;
class UTextBlock;
class UForeverThrowListButtonWidget;
struct FThrowListItem;

// 长按3弹出的投掷物选择列表：每组可投掷的物品一个按钮(名称+数量)，点击激活该物品并关闭。
// 打开期间世界慢放到0.1倍；松开3键时如果列表还开着，说明没点任何按钮，只关闭。
// 逻辑都在C++里，外观由Blueprint子类(WBP_ThrowList)设计，需要的控件见下面的BindWidget成员和
// ForeverThrowListWidget.md。
UCLASS()
class FOREVER_API UForeverThrowListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 打开列表。世界已经被别的系统冻结(时间缩放<0.01，比如暂停面板/谜题)时不打开，返回false。
	bool Open(const TArray<FThrowListItem>& items, const FString& activeType);

	// 完全关闭：还原时间缩放和输入模式。幂等。
	void Close();

	// 列表正在显示，或者点击按钮后正在等3键真正松开(见Select)——这两个阶段输入都被控件占着。
	bool IsOpen() const { return bOpen || bAwaitingRelease; }

	// 点击了某个按钮：激活该投掷物，列表隐藏、世界恢复正常速度，但**输入模式保持UI Only**，
	// 等3键真正松开(NativeOnKeyUp)才切回游戏。点击时3键通常还按着，现在切回游戏会清空按键状态，
	// 系统的按键重复随后会让Pawn收到一次假的"按下"、又触发长按弹出列表；留在UI Only里，这些
	// 重复按键都被控件吃掉，不会到Pawn。代价是点击后到松开3之前不能移动/转视角。
	void Select(const FString& type);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	// UI Only输入模式下Enhanced Input收不到按键，松开3键要在这里自己拦。
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 按钮行的类(WBP_ThrowListButton)，在WBP_ThrowList的类默认值里指定。
	UPROPERTY(EditDefaultsOnly, Category = "ThrowList")
	TSubclassOf<UForeverThrowListButtonWidget> buttonClass;

	// 按钮行之间的竖直间距：每行的上下各留这么多(UE单位)，相邻两行之间的缝隙是它的两倍。和
	// UMeetOptionWidget/USectionOptionWidget同一种做法(在C++里给每行的ScrollBoxSlot设Padding)。
	UPROPERTY(EditDefaultsOnly, Category = "ThrowList")
	float rowVerticalPadding = 6.f;

	// 放按钮行的滚动框，行多了自己滚动。
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ListContainer;

	// 没有投掷物时显示的提示文字；可以不摆(不摆则列表为空时什么都不显示)。
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyText;

private:
	// 把世界时间缩放还原成打开前的值(仅当列表还在显示、且缩放仍是我们设的值)。
	void RestoreTimeDilation();

	// 列表在显示(世界慢放中)。
	bool bOpen = false;
	// 已经点击了按钮、列表已隐藏，但还在UI Only里等3键松开。
	bool bAwaitingRelease = false;
	double awaitStartRealTime = 0.0;
	float previousDilation = 1.f;
};
