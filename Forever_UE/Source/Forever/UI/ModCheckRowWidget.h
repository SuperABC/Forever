#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ModCheckRowWidget.generated.h"

class UTextBlock;
class UCheckBox;

// 一行=一个mod id的勾选框，替代老工程CheckEnable——该dump本身是损坏的(内容实际是
// BagPanel.txt的ClearSelect函数被误拷贝进来)，结构只能从ModPanel.txt对它的引用反推。
// 被依赖(bLocked)且已勾选时整行(SetIsEnabled(false))变灰、勾选框不能取消；被依赖但还没勾选时
// 文字标红、勾选框保持未勾选且可点。切换时调
// ForeverConfigBridgeSubsystem::SetModEnabled，再回调InOnChanged(拥有者
// ModPanelWidget::RefreshAllRows)做级联刷新——禁用/启用任何一个id都可能让别的id的
// 锁定状态跟着变，所以不能只刷新自己这一行。
UCLASS()
class FOREVER_API UModCheckRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// InDependents：被依赖时是谁在依赖它，鼠标悬停在这一行(含勾选框)上时用提示框列出来；没被依赖时为空，不显示提示。
	void SetRowData(const FString& InDllPath, const FString& InConceptKey, const FString& InModId,
		bool bEnabled, bool bLocked, const TArray<FString>& InDependents, TFunction<void()> InOnChanged);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleCheckChanged(bool bIsChecked);

	// 依赖列表的悬停提示控件：深色背景+放大字号的文本。
	UWidget* BuildDependentsToolTip(const FString& Text);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ModIdLabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCheckBox> EnableCheckBox;

private:
	FString dllPath;
	FString conceptKey;
	FString modId;
	TFunction<void()> onChanged;

	// 文字默认颜色，首次SetRowData时从控件上记下来，标红后能还原。
	FSlateColor defaultLabelColor;
	bool bDefaultLabelColorCaptured = false;
};
