#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryWidget.generated.h"

class UScrollBox;
class UButton;
class UTextBlock;
class UAssetButtonWidget;
class Asset;
class Player;
class Room;

// 背包UI落地：对应老蓝图BagPanel，这次要求"逻辑全部写在C++基类里，蓝图只做纯布局子类"，
// 老蓝图BagPanel完全没有对应的C++类，这次重写成C++——见Source/Core/player/asset.md和
// .dump/BagPanel.txt还原出的交互模型。
//
// 两栏布局(不是tab切换，同时可见)：
// - 身体栏(BodyList+BodyLabel)：5个标签按钮(LeftHand/RightHand/BackPack/LeftShoulder/
//   RightShoulder)切换"当前显示哪个槽位的内容"，点哪个按钮就是ClickBody的Part——每个
//   标签的根视图永远只显示"这个槽位当前那一个Asset"(0或1行)，哪怕这个Asset本身是容器
//   (比如BackPack槽位放的PlayerBag)，根视图也不自动展开它的内容，用户明确要求"BackPack
//   标签不要自动展开容器内容，左右手也不要自动展开"——5个标签在这一点上完全对称，没有
//   任何特殊分支。
// - 房间栏(RoomList+RoomLabel)：固定显示Player::GetCurrentRoom()的内容(平铺表)。
// 两栏各自可以"进入"选中的容器再深一层：双击一个容器行(不管它是槽位根视图里的那一个
// Asset，还是已经进入的容器内部找到的另一个容器)进入(压栈)，单击列表最前面的".."占位行
// 退出(出栈)——用户明确要求用双击而不是Enter/Exit按钮，但原生
// `NativeOnMouseButtonDoubleClick`会被行内的`Button`拦截、传不到这一层，改成在
// `UAssetButtonWidget::HandleClicked`里自己按时间戳判双击，见该类头文件注释；"返回
// 上一级"单击即触发，不需要凑双击。
//
// Use/Drop/Pick三个按钮对"当前选中项"生效，和老工程语义完全一致(只是身体子视图从3个扩到
// 5个)：Use=消耗selectedBody；Drop=selectedBody从当前身体侧解析出的容器/槽位移到房间
// 地上(cube)；Pick=selectedRoom从房间移入当前身体侧解析出的容器/槽位。武器的"激活/卸下"
// 完全由数字键1/2驱动(AForeverCharacter::ActivateShoulderWeapon)，这个Widget不需要知道
// 武器系统的任何状态，见Source/Core/player/player.h"武器只挂肩膀"一节。
UCLASS()
class FOREVER_API UForeverInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// B键(ToggleInventory)：可见就ClosePanel，不可见就InitPanel。
	void TogglePanel();

	// 由UAssetButtonWidget::HandleClicked调用——记录选中态，房间栏/身体栏各自独立，
	// 复刻老工程SelectBag/SelectRoom。
	void SelectRow(UAssetButtonWidget* row);
	// 由UAssetButtonWidget::HandleClicked在检测到双击(按时间戳判定，见该类注释)且
	// row->GetAsset()->IsContainer()==true时调用，按row所属的那一侧压栈进入。
	void EnterRow(UAssetButtonWidget* row);
	// 由UAssetButtonWidget::HandleClicked在row->IsBackRow()==true时调用(单击即触发，
	// 不需要双击)，按row所属的那一侧出栈退出。
	void ExitRow(UAssetButtonWidget* row);

protected:
	virtual void NativeConstruct() override; // 默认Collapsed+绑定全部按钮的OnClicked

	UFUNCTION() void HandleClickLeftHand();
	UFUNCTION() void HandleClickRightHand();
	UFUNCTION() void HandleClickBackPack();
	UFUNCTION() void HandleClickLeftShoulder();
	UFUNCTION() void HandleClickRightShoulder();
	UFUNCTION() void HandleUse();
	UFUNCTION() void HandleDrop();
	UFUNCTION() void HandlePick();
	UFUNCTION() void HandleClose();
	UFUNCTION() void HandleReset();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> BodyList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> RoomList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BodyLabel;

	// 当前房间地址(或"不在房间中")+已进入的子容器面包屑，见RefreshRoom()。
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RoomLabel;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> LeftHandButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> RightHandButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> BackPackButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> LeftShoulderButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> RightShoulderButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> UseButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> DropButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> PickButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ResetButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CloseButton;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UAssetButtonWidget> AssetButtonClass;

private:
	void InitPanel();
	void ClosePanel();
	void ClickBody(const FString& part);
	void ResetRoom();
	void ClearSelect();
	void RefreshBody();
	void RefreshRoom();

	Player* GetPlayer() const;

	// bodyContainerStack非空则返回栈顶(已经双击进入过的容器)，否则nullptr——身体标签的
	// 根视图永远不自动展开，哪怕当前槽位那一个Asset本身是容器，见类注释。
	Asset* ResolveBodyContainer() const;
	// roomContainerStack非空则返回栈顶，否则nullptr(房间根列表本身不是容器，是
	// Room::GetAssets()这个平铺表)。
	Asset* ResolveRoomContainer() const;

	static FString BodyPartToPath(const FString& part);
	static FString BuildBreadcrumb(const FString& root, const TArray<Asset*>& stack);

	FString currentBodyPart = TEXT("BackPack");
	TWeakObjectPtr<UAssetButtonWidget> selectedBody;
	TWeakObjectPtr<UAssetButtonWidget> selectedRoom;
	bool lastSelectionIsRoom = false;

	TArray<Asset*> bodyContainerStack;
	TArray<Asset*> roomContainerStack;
};
