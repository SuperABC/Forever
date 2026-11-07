#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "player/canvas.h"
#include "player/phone.h"

#include "PhoneWidget.generated.h"

class UImage;
class UTexture2D;

// 手机宿主：把Core层Phone实例跑起来、画布贴到Image上、转发键鼠事件。由AForeverPlayerController常驻持有，
// P键(TogglePhone)开关。与PuzzleWidget的差异(生命周期跨开关保持、画布尺寸取DisplayImage)见PhoneWidget.md。
UCLASS()
class FOREVER_API UPhoneWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 由AForeverPlayerController::TogglePhone调用(P键)：可见就ClosePhone，不可见就
	// OpenPhone。两个分支都不会delete/重建phone，见类注释第1点。
	void TogglePhone();

protected:
	virtual void NativeConstruct() override; // 默认Collapsed，没打开手机就不显示
	virtual void NativeDestruct() override;  // Widget自己被销毁时才真正delete phone
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 命名避开跟Core层Canvas类同名，Blueprint子类里摆一个手机形状(不用铺满全屏，见类注释
	// 第2点)的Image控件绑到这个名字上。
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> DisplayImage;

private:
	void OpenPhone();  // 首次调用才New phone+canvas.Init；SetVisibility(Visible)+UIOnly输入模式
	void ClosePhone(); // SetVisibility(Collapsed)+GameOnly输入模式；不delete phone/canvas

	// 把canvas当前像素缓冲区贴到DisplayImage上，照抄UPuzzleWidget::BlitCanvasToImage。
	void BlitCanvasToImage();

	// DisplayImage在屏幕上的实际像素footprint(GetAbsoluteSize()已经包含DPI缩放)，查不到
	// (比如Widget还没Visible过、几何信息还没算出来)时退回kFallbackPhoneWidth/Height。
	FIntPoint ComputeCanvasSize() const;

	Canvas canvas;
	Phone* phone = nullptr; // 跨越开/关手机持续存在，只在NativeDestruct里delete

	UPROPERTY()
	TObjectPtr<UTexture2D> texture;

	static constexpr int kFallbackPhoneWidth = 480;
	static constexpr int kFallbackPhoneHeight = 640;
};
