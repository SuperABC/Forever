#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StoryPanelWidget.generated.h"

class UScrollBox;
class UButton;
class UPathRowWidget;
class UModCheckRowWidget;

// Story screen：添加/删除.script文件，全部一起加载(不是多选一激活)，结构同ResourcePanel的ScrollBox+PathRow。
UCLASS()
class FOREVER_API UStoryPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Config")
	void RefreshRows();

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleAddScriptClicked();

	void RefreshMainStoryList();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ScriptList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> AddScriptButton;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UPathRowWidget> PathRowClass;

	// 主线剧情mod单选列表：列出已启用的script mod，只能选一个作为config.json的main_story。
	// WBP里放一个叫MainStoryList的ScrollBox，行用WBP_ModCheckRow（ModCheckRowClass）；两者缺一就不显示。
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> MainStoryList;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UModCheckRowWidget> ModCheckRowClass;
};
