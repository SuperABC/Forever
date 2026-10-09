#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForeverStoryPanelWidget.generated.h"

class UScrollBox;
class UButton;
class UForeverPathRowWidget;

// Story screen：添加/删除.script文件，全部一起加载(不是"多选一激活")——老工程没有这个
// screen，纯新设计，没有.dump precedent可以照抄，结构照抄ResourcePanel的"ScrollBox+
// PathRow"模式。
UCLASS()
class FOREVER_API UForeverStoryPanelWidget : public UUserWidget
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
	TSubclassOf<UForeverPathRowWidget> PathRowClass;
};
