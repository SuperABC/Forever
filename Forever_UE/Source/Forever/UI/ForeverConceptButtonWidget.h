#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForeverConceptButtonWidget.generated.h"

class UTextBlock;
class UButton;

// Mod screen中间一列固定的21个按钮里的一个，每个对应一个concept(按
// Source/Core/common/loader.h GetModConceptDescriptors()的顺序由ForeverModPanelWidget
// 在NativeConstruct里动态生成，不需要在Blueprint里手摆21个按钮)。点击后通知owner切换
// 右侧ScrollBox显示哪个concept的mod列表。
UCLASS()
class FOREVER_API UForeverConceptButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupButton(const FString& InConceptKey, TFunction<void(const FString&)> InOnClicked);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ConceptLabel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SelectButton;

private:
	FString conceptKey;
	TFunction<void(const FString&)> onClicked;
};
