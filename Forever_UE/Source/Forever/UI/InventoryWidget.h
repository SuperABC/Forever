#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/InventoryTransfer.h"
#include "UI/InventoryButtonStyle.h"
#include "InventoryWidget.generated.h"

class UScrollBox;
class UButton;
class UTextBlock;
class UAssetButtonWidget;
class Asset;
class Player;
class Room;

// 背包UI：逻辑在C++基类里，WBP_Inventory只做布局。物品移动全靠拖拽，规则见InventoryTransfer.h，
// 区域划分和交互见InventoryWidget.md。武器的激活/卸下由数字键驱动(AForeverCharacter::ActivateShoulderWeapon)，本Widget不管。
UCLASS()
class FOREVER_API UInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// B键(ToggleInventory)：可见就ClosePanel，不可见就InitPanel。
	void TogglePanel();

	// 由UAssetButtonWidget调用：单击选中、双击进入容器、单击".."退出。
	void SelectRow(UAssetButtonWidget* row);
	void EnterRow(UAssetButtonWidget* row);
	void ExitRow(UAssetButtonWidget* row);

	// 由UAssetButtonWidget在行上按下左键时调用：面板接管鼠标(捕获)，之后的移动/松开都由面板处理——
	// 没拖动就是单击(UAssetButtonWidget::Click)，拖动超过阈值就开始拖拽，见NativeOnMouseButtonUp。
	FReply BeginRowPress(UAssetButtonWidget* row, const FPointerEvent& InMouseEvent);

	// 列表行/拖拽框的文字：类型名 [xN] [容器]。
	static FString DescribeAsset(const Asset* asset, int amount);

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UFUNCTION() void HandleUse();
	UFUNCTION() void HandleClose();

	// 容器列表：选中槽位里容器的内容(及进入的子容器)。
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> BodyList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> RoomList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BodyLabel;

	// 当前房间地址+已进入的子容器面包屑，见RefreshRoom()。
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RoomLabel;

	// 附近列表。BindWidgetOptional：WBP_Inventory里还没放时不影响蓝图编译，只是不显示附近。
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> NearbyList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NearbyLabel;

	// 操作失败的提示(可选)；没放时回退成屏幕调试信息。
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> LeftHandButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> RightHandButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> BackPackButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> LeftShoulderButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> RightShoulderButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> UseButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CloseButton;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UAssetButtonWidget> AssetButtonClass;

private:
	void InitPanel();
	void ClosePanel();
	void ClickSlot(EInvZone zone);
	void ClearSelect();
	void RefreshAll();
	void RefreshSlots();
	void RefreshBody();
	void RefreshRoom();
	void RefreshNearby();
	void ShowMessage(const FString& text);

	// 复用行对象的列表填充：返回回收的行数。bBack=true时最前面插".."行。
	void FillList(UScrollBox* list, TArray<TObjectPtr<UAssetButtonWidget>>& pool, EInvZone zone,
		Asset* parent, bool bBack, const TArray<Asset*>& assets);

	Player* GetPlayer() const;
	class UForeverAssetFrameworkComponent* GetAssetFramework() const;

	// 槽位按钮 <-> 区域
	UButton* SlotButton(EInvZone zone) const;
	bool FindSlotAt(const FVector2D& screenPosition, EInvZone& outZone) const;
	Asset* GetSlotAsset(EInvZone zone) const;

	// 容器列表当前显示的容器：进入了子容器=栈顶，否则=选中槽位里的容器(没有则nullptr)。
	Asset* ResolveBodyContainer() const;
	Asset* ResolveRoomContainer() const;
	Asset* ResolveNearbyContainer() const;

	static FString BuildBreadcrumb(const FString& root, const TArray<Asset*>& stack);
	static FString SlotName(EInvZone zone);

	// 把鼠标位置翻译成拖拽目标区域；不在任何区域上返回false。
	bool ResolveDropTarget(const FVector2D& screenPosition, FInvDest& outDest) const;

	EInvZone currentSlot = EInvZone::Back;
	TWeakObjectPtr<UAssetButtonWidget> selectedBody;
	TWeakObjectPtr<UAssetButtonWidget> selectedOther; // 房间/附近列表里的选中行，仅高亮

	TArray<Asset*> bodyContainerStack;
	TArray<Asset*> roomContainerStack;
	TArray<Asset*> nearbyContainerStack;

	TArray<TObjectPtr<UAssetButtonWidget>> bodyPool;
	TArray<TObjectPtr<UAssetButtonWidget>> roomPool;
	TArray<TObjectPtr<UAssetButtonWidget>> nearbyPool;

	// 槽位按钮原本的文字标签(首次读到的WBP里的文字)，显示时拼成"标签: 物品"。
	// 下标=槽位区域的枚举值(槽位是EInvZone的前5个)。
	FString slotBaseLabels[5];
	bool bSlotLabelsCaptured = false;

	// 在槽位按钮上按下的那个槽位：拖动了就开始拖拽，没拖动松开就是点击。
	bool bPressActive = false;  // 槽位或列表行上按着左键(面板已捕获鼠标)
	bool bSlotPressed = false;  // 其中按下的是槽位(而不是列表行)
	EInvZone pressedSlot = EInvZone::Back;
	TWeakObjectPtr<UAssetButtonWidget> pressedRow;
	FInvSource pressSource;     // 按下时的拖拽源，松手时由Transfer重新校验

	// 槽位按钮被设成HitTestInvisible后原生的悬停/按下样式不再生效，这里按鼠标位置手动给
	// 按钮上色(悬停变亮、按下更深)。
	bool bSlotHovered = false;
	EInvZone hoveredSlot = EInvZone::Back;
	void UpdateSlotTints();
	FInvButtonStyleState slotStyles[5]; // 下标=槽位区域的枚举值

	// 槽位的拖拽不走UMG DragDropOperation(它的拖拽框会从发起控件左上角滑向鼠标)，面板自己捕获
	// 鼠标、用一个贴着鼠标的框(复用AssetButtonClass的行)实现，松手时PerformDrop结算。
	bool bSlotDragging = false;
	bool bSlotDragAll = false;
	FVector2D slotPressPos = FVector2D::ZeroVector;
	UPROPERTY() TObjectPtr<UAssetButtonWidget> dragGhost;
	void CreateGhost(Asset* asset, bool bAll);
	void UpdateGhost(const FVector2D& screenPosition);
	void DestroyGhost();
	// 按松手位置确定目标区域并交给InventoryTransfer::Transfer。
	void PerformDrop(const FInvSource& source, bool bAll, const FVector2D& screenPosition);
};
