#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForeverIdentifyTagWidget.generated.h"

class UVerticalBox;

// 识别模式下挂在人物头上的文字标签(放进世界空间的UWidgetComponent)：任意多行，最下面一行贴着
// 控件底边，行数增加时往上长——所以把控件底边中点对齐到人头上方，最后一行始终在头上方一点。
// 白色文字加黑色描边。控件树在C++里搭(没有WBP也能用)，见ForeverIdentifyTagWidget.md。
UCLASS()
class FOREVER_API UForeverIdentifyTagWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	// 用这些行替换当前内容，第一个元素在最上面，最后一个在最下面。resolutionScale是分辨率倍数：
	// 字号乘它(描边宽度不乘，仍是整数像素)，调用方同时把控件绘制尺寸乘它、世界缩放除以它，
	// 画面大小不变，但描边相对字形变细(OutlineSize是整数，最小1，没法直接设0.2)。
	void SetLines(const TArray<FString>& lines, float resolutionScale = 1.f);

	// 字号(Slate单位，世界里的实际大小还要乘WidgetComponent的缩放)和描边宽度。
	UPROPERTY(EditDefaultsOnly, Category = "Identify")
	int32 fontSize = 32;

	UPROPERTY(EditDefaultsOnly, Category = "Identify")
	int32 outlineSize = 3;

private:
	UPROPERTY()
	TObjectPtr<UVerticalBox> linesBox;
};
