#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ResourcePanelWidget.generated.h"

class UScrollBox;
class UButton;
class UPathRowWidget;

// Resource screen：资源文件夹列表，对应Config::AddResourcePath发现的文件夹，一个文件夹同时覆盖
// .layout和.script。.script列表在StoryPanelWidget。
UCLASS()
class FOREVER_API UResourcePanelWidget : public UUserWidget
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
	TSubclassOf<UPathRowWidget> PathRowClass;
};
