#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForeverThrowListButtonWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FOnThrowListButtonSelected, const FString& /*type*/);

// 投掷物选择列表里的一行(一组可投掷物)：一个按钮+一行文字"资产id  x数量"。逻辑都在C++里，
// Blueprint子类(WBP_ThrowListButton)只需要摆好名字对得上的控件并设计外观，见ForeverThrowListWidget.md。
UCLASS()
class FOREVER_API UForeverThrowListButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 由UForeverThrowListWidget::Open创建完实例后立刻调用一次。
	void Setup(const FString& inType, int32 inCount, bool bInActive);

	// 点击时回调(带type)，由列表控件绑定。
	FOnThrowListButtonSelected onSelected;

	const FString& GetType() const { return type; }

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleClicked();

	// 当前激活的投掷物那一行的按钮底色，和其它行的底色；在WBP_ThrowListButton的类默认值里调。
	UPROPERTY(EditDefaultsOnly, Category = "ThrowList")
	FLinearColor activeColor = FLinearColor(0.2f, 0.45f, 0.8f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "ThrowList")
	FLinearColor inactiveColor = FLinearColor(0.25f, 0.25f, 0.25f, 1.f);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> Button;

	// 显示"资产id  x数量"。
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> Label;

private:
	FString type;
};
