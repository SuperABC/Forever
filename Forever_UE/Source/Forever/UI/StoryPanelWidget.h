#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StoryPanelWidget.generated.h"

class UScrollBox;
class UButton;
class UPathRowWidget;

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

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ScriptList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> AddScriptButton;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UPathRowWidget> PathRowClass;
};
