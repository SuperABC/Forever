#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/InventoryTransfer.h"
#include "UI/InventoryButtonStyle.h"
#include "AssetButtonWidget.generated.h"

class UButton;
class UTextBlock;
class UForeverInventoryWidget;
class Asset;

// 背包列表里的一行——对应老蓝图AssetButton_C。
//
// 内部的`Button`只负责外观(选中高亮背景色)，构造时设成HitTestInvisible：这样鼠标事件
// 直接落到这一行自己身上(`SButton`会吞掉按下事件，导致拖拽检测和原生双击都到不了这一层)，
// 按下只通知面板(UForeverInventoryWidget::BeginRowPress)，由面板捕获鼠标统一处理单击/拖拽，逻辑都在C++
// 基类里，Blueprint子类只需要摆一个命名Button的Button + 命名Text的TextBlock。
//
// - 单击(按下后没有拖动就松开)=选中(容器/普通行)；"返回上一级"占位行单击直接退出；
// - 双击(第二次点击落在kDoubleClickInterval秒以内)=容器行则进入，见
//   UForeverInventoryWidget::EnterRow/ExitRow；
// - 按下并拖动=开始拖拽(面板自己的贴着鼠标的框，不用UMG DragDropOperation)，见UForeverInventoryWidget。
UCLASS()
class FOREVER_API UAssetButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 由UForeverInventoryWidget刷新列表时调用(行对象会被复用，所以每次刷新都会重新Setup)。
	// @inAsset: 非owning，真正所有权在槽位/容器/房间/Player
	// @inZone: 这一行属于哪个列表(Container/Room/Nearby)
	// @inParent: 这一行所在的容器(根视图为nullptr)，拖拽时作为源的parent
	void Setup(UForeverInventoryWidget* inOwner, Asset* inAsset, EInvZone inZone, Asset* inParent);

	// 设置成"返回上一级"占位行(asset为空，不能被选中/拖拽)。
	void SetupAsBack(UForeverInventoryWidget* inOwner, EInvZone inZone);

	Asset* GetAsset() const { return asset; }
	EInvZone GetZone() const { return zone; }
	Asset* GetParentContainer() const { return parentContainer; }
	bool IsBackRow() const { return bIsBackRow; }

	void SetRowSelected(bool bSelected);

	// 单击逻辑(返回行=退出，容器行0.3s内第二次=进入，其余=选中)。由面板在没拖动就松开时调用。
	void Click();
	// 面板松开鼠标后调用：按松开位置重算悬停，还原按下样式(面板捕获鼠标期间收不到MouseLeave)。
	void FinishPress(const FVector2D& screenPosition);
	bool IsUnderScreenPosition(const FVector2D& screenPosition) const { return GetCachedGeometry().IsUnderLocation(screenPosition); }

	// 直接设置文字(拖拽框复用这个控件时用，不关联任何Asset)。
	void SetLabel(const FString& label);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text;

private:

	// 两次点击的间隔在这个阈值以内算双击——0.3秒是桌面系统常见的双击判定区间。
	static constexpr double kDoubleClickInterval = 0.3;

	TWeakObjectPtr<UForeverInventoryWidget> owner;
	Asset* asset = nullptr; // 非owning，IsBackRow()==true时恒为nullptr
	Asset* parentContainer = nullptr;
	EInvZone zone = EInvZone::Container;
	bool bIsBackRow = false;
	double lastClickTime = -1000.0;

	// Button被设成HitTestInvisible后原生悬停/按下样式不再生效，手动复现，见InventoryButtonStyle.h。
	FInvButtonStyleState styleState;
	bool bHovered = false;
	void UpdateButtonState(bool bPressed);
};
