#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "player/canvas.h"
#include "player/phone.h"

#include "PhoneWidget.generated.h"

class UImage;
class UTexture2D;

// 手机(Phone)系统落地：跟UPuzzleWidget同一套Widget基类惯例(逻辑全在C++基类，Blueprint
// 子类只摆布局)，把Core层的Phone实例跑起来、把它的画布贴到一张Image上，转发UE的键鼠事件。
// 由AForeverPlayerController常驻持有一份实例，P键(TogglePhone)控制开关，见
// AForeverPlayerController::SetupInputComponent。
//
// 跟UPuzzleWidget两个关键差异：
// 1) 生命周期——Puzzle每次StartPuzzle都new一个新实例、结束时delete；Phone要跨越
//    "开手机->关手机->再开手机"保持多任务状态(桌面/正在跑的App/最近任务列表)，所以
//    phone/canvas只在第一次打开时创建，TogglePhone()此后只切换Visibility+输入模式，不
//    delete/重建，只有Widget自己被销毁(NativeDestruct)时才真正释放。
// 2) 画布分辨率——Puzzle用GameViewport的真实屏幕分辨率(填满全屏，1:1)；Phone改成查
//    DisplayImage这个子控件自己的屏幕像素尺寸(同样1:1，不铺满全屏，只是取景范围换成
//    这个子控件而不是整个视口)，见ComputeCanvasSize()。
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
