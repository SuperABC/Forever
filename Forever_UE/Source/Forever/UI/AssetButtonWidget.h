#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/InventoryTransfer.h"
#include "UI/InventoryButtonStyle.h"
#include "AssetButtonWidget.generated.h"

class UButton;
class UTextBlock;
class UInventoryWidget;
class Asset;

// 背包列表里的一行。内部Button只负责外观(选中高亮)，构造时设成HitTestInvisible(SButton会吞掉按下事件)，
// 按下只通知面板(UInventoryWidget::BeginRowPress)，由面板统一处理：单击=选中，双击容器行=进入，
// "返回上一级"占位行单击退出，按下拖动=开始拖拽。
UCLASS()
class FOREVER_API UAssetButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 由UInventoryWidget刷新列表时调用(行对象会被复用，所以每次刷新都会重新Setup)。
	// @inAsset: 非owning，真正所有权在槽位/容器/房间/Player
	// @inZone: 这一行属于哪个列表(Container/Room/Nearby)
	// @inParent: 这一行所在的容器(根视图为nullptr)，拖拽时作为源的parent
	void Setup(UInventoryWidget* inOwner, Asset* inAsset, EInvZone inZone, Asset* inParent);

	// 设置成"返回上一级"占位行(asset为空，不能被选中/拖拽)。
	void SetupAsBack(UInventoryWidget* inOwner, EInvZone inZone);

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

	TWeakObjectPtr<UInventoryWidget> owner;
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
