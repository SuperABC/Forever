#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AssetButtonWidget.generated.h"

class UButton;
class UTextBlock;
class UForeverInventoryWidget;
class Asset;

// 背包列表里的一行——对应老蓝图AssetButton_C，双击进入容器这个交互本身保留(用户明确要求)。
// **不用`NativeOnMouseButtonDoubleClick`**——实测这个事件到不了这一层：`Button`底层的
// `SButton`对"双击的第二次按下"默认按普通`OnMouseButtonDown`处理并直接标记
// `Handled()`，事件在`Button`这一级就被吃掉，永远不会冒泡到外层`UUserWidget`的
// `NativeOnMouseButtonDoubleClick`。改成在`Button::OnClicked`回调(`HandleClicked`)里
// 自己按时间戳判断"这次点击离上次点击够不够近"，效果等价于双击检测，但不依赖会被
// `Button`拦截的那个原生事件——这和老蓝图`AssetButton_C`的Pending/Current计时器本质上
// 是同一种手段(时间戳判双击)，只是不需要专门的Tick，靠`FPlatformTime::Seconds()`
// 现算现比。逻辑全部在C++基类里，Blueprint子类只需要摆一个命名Button的Button + 命名
// Text的TextBlock。
//
// 单击=选中(容器/普通行)或直接退出("返回上一级"占位行不需要双击，见HandleClicked)；
// 双击(第二次点击落在kDoubleClickInterval秒以内)=容器行则进入，见
// UForeverInventoryWidget::EnterRow/ExitRow。
UCLASS()
class FOREVER_API UAssetButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 由UForeverInventoryWidget::RefreshBody/RefreshRoom创建完实例后立刻调用一次。
	// @inAsset: 非owning，真正所有权在槽位/容器/房间
	// @inIsRoomSide: true=这一行来自房间栏，false=来自身体栏——决定点击时通知
	//   UForeverInventoryWidget::SelectRow该记到哪一侧的选中态
	void Setup(UForeverInventoryWidget* inOwner, Asset* inAsset, bool inIsRoomSide);

	// 设置成"返回上一级"占位行(asset为空，不能被选中/Use/Drop/Pick)——RefreshBody/
	// RefreshRoom在对应容器栈非空时于列表最前面插一行，单击退出当前容器(不需要双击，见
	// HandleClicked)。
	void SetupAsBack(UForeverInventoryWidget* inOwner, bool inIsRoomSide);

	Asset* GetAsset() const { return asset; }
	bool IsRoomSide() const { return isRoomSide; }
	bool IsBackRow() const { return bIsBackRow; }

	void SetRowSelected(bool bSelected);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text;

private:
	// 两次点击的间隔在这个阈值以内算双击——0.3秒是桌面系统常见的双击判定区间。
	static constexpr double kDoubleClickInterval = 0.3;

	TWeakObjectPtr<UForeverInventoryWidget> owner;
	Asset* asset = nullptr; // 非owning，IsBackRow()==true时恒为nullptr
	bool isRoomSide = false;
	bool bIsBackRow = false;
	double lastClickTime = -1000.0;
};
