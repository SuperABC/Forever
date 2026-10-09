#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForeverResourcePanelWidget.generated.h"

class UScrollBox;
class UButton;
class UForeverPathRowWidget;

// Resource screen：只保留老工程ResourcePanel(.dump/ResourcePanel.txt)的资源文件夹列表那
// 一半——.script列表移到新增的ForeverStoryPanelWidget(Story screen)，这里的ResourceList
// 对应Config::AddResourcePath发现的文件夹，一个文件夹同时覆盖.layout和.script(见
// config.md"DLL级依赖声明"旁边新增的说明)。
UCLASS()
class FOREVER_API UForeverResourcePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Config")
	void RefreshRows();

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleAddResourceClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ResourceList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> AddResourceButton;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UForeverPathRowWidget> PathRowClass;
};
