#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PathRowWidget.generated.h"

class UTextBlock;
class UButton;

// 一行=一个路径+删除按钮，Story screen和Resource screen共用。SetupRow传入的OnDelete回调由拥有者
// (StoryPanelWidget/ResourcePanelWidget)决定调哪个Config mutator，本类不区分用途。
UCLASS()
class FOREVER_API UPathRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupRow(const FString& InPath, TFunction<void(const FString&)> InOnDelete);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleDeleteClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PathLabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> DeleteButton;

private:
	FString path;
	TFunction<void(const FString&)> onDelete;
};
