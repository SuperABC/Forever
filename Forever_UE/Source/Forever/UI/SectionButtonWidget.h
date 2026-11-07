#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SectionButtonWidget.generated.h"

class UButton;
class UTextBlock;
class AForeverFrameworkActor;

// USectionOptionWidget列表里的单个分支选项行，与MeetButtonWidget字段/回调目标不同，不复用同一个类。
// Blueprint子类只需要摆命名Button和命名Text。
UCLASS()
class FOREVER_API USectionButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 由USectionOptionWidget::ShowOptions创建每一行时调用一次。index是当前分支Section::GetOptions()里的下标，
	// text是已求值的选项文本(求值在UForeverStoryFrameworkComponent::ShowDialog里完成)。
	void Setup(AForeverFrameworkActor* inFramework, int32 inIndex, const FString& text);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text;

private:
	TWeakObjectPtr<AForeverFrameworkActor> framework;
	int32 index = -1;
};
