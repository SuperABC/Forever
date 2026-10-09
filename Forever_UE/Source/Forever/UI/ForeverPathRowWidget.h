#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForeverPathRowWidget.generated.h"

class UTextBlock;
class UButton;

// 一行=一个路径+删除按钮，Story screen(脚本列表)和Resource screen(资源文件夹列表)共用，
// 替代老工程PathItem(.dump/PathItem.txt：文本+DeleteButton)。SetupRow传入的OnDelete回调
// 由拥有者(StoryPanelWidget/ResourcePanelWidget)决定删除时具体调哪个Config mutator，这个
// 类本身不知道自己是Story行还是Resource行，不需要为两种用途各写一个子类。
UCLASS()
class FOREVER_API UForeverPathRowWidget : public UUserWidget
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
