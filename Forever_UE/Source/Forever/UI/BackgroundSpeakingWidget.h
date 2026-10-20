#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BackgroundSpeakingWidget.generated.h"

class UTextBlock;

// 后台对话的展示框(屏幕上方)：和USectionSpeakingWidget一样是发言者+内容两行，但没有点击/
// 键盘推进——只靠UForeverStoryFrameworkComponent的计时器结束。整个控件HitTestInvisible，不挡
// 鼠标、不抢焦点，玩家照常操控角色。Blueprint子类(WBP_BackgroundSpeaking)只需要摆Speaker/
// Content两个TextBlock，位置放在屏幕上方。见BackgroundSpeakingWidget.md。
UCLASS()
class FOREVER_API UBackgroundSpeakingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 显示一条后台台词。speaker为空时隐藏发言者那一行。
	void ShowLine(const FString& speaker, const FString& content);

	void HideLine();

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Speaker;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Content;
};
