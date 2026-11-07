#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ModCheckRowWidget.generated.h"

class UTextBlock;
class UCheckBox;

// 一行=一个mod id的勾选框。被依赖(bLocked)且已勾选时整行变灰不能取消；被依赖但未勾选时文字标红。
// 切换时调ForeverConfigBridgeSubsystem::SetModEnabled，再回调拥有者ModPanelWidget::RefreshAllRows做级联刷新。
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
