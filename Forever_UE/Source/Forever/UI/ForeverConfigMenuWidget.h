#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForeverConfigMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UForeverStoryPanelWidget;
class UForeverModPanelWidget;
class UForeverResourcePanelWidget;

// 配置界面顶层，替代老工程StartMenu(.dump/StartMenu.txt)：三screen用plain SetVisibility
// 切换(不是tab-control widget，和老工程ModPanel/ResourcePanel的sibling panel切换手法一致，
// 只是从两个扩到三个)，StartButton调Bridge::ValidateAndStartGame，失败时在FailureMessage
// (左上角，初始Collapsed)显示拒绝原因。由ForeverMenuController::BeginPlay()
// CreateWidget+AddToViewport创建，见该类。
UCLASS()
class FOREVER_API UForeverConfigMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UFUNCTION() void HandleStoryTabClicked();
	UFUNCTION() void HandleModTabClicked();
	UFUNCTION() void HandleResourceTabClicked();
	UFUNCTION() void HandleStartClicked();

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UForeverStoryPanelWidget> StoryPanel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UForeverModPanelWidget> ModPanel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UForeverResourcePanelWidget> ResourcePanel;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> StoryTabButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ModTabButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ResourceTabButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> StartButton;

	// 左上角失败提示，初始Collapsed，ValidateAndStartGame失败时显示拒绝原因。
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> FailureMessage;

private:
	enum class ETab { Story, Mod, Resource };
	void ShowOnly(ETab Tab);
};
