#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForeverModCheckRowWidget.generated.h"

class UTextBlock;
class UCheckBox;

// 一行=一个mod id的勾选框，替代老工程CheckEnable——该dump本身是损坏的(内容实际是
// BagPanel.txt的ClearSelect函数被误拷贝进来)，结构只能从ModPanel.txt对它的引用反推。
// bLocked时勾选框强制勾中+整行(SetIsEnabled(false))禁用交互，切换时调
// ForeverConfigBridgeSubsystem::SetModEnabled，再回调InOnChanged(拥有者
// ForeverModPanelWidget::RefreshAllRows)做级联刷新——禁用/启用任何一个id都可能让别的id的
// 锁定状态跟着变，所以不能只刷新自己这一行。
UCLASS()
class FOREVER_API UForeverModCheckRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRowData(const FString& InDllPath, const FString& InConceptKey, const FString& InModId,
		bool bEnabled, bool bLocked, TFunction<void()> InOnChanged);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleCheckChanged(bool bIsChecked);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ModIdLabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCheckBox> EnableCheckBox;

private:
	FString dllPath;
	FString conceptKey;
	FString modId;
	TFunction<void()> onChanged;
};
