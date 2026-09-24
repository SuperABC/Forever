#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "player/canvas.h"
#include "player/puzzle.h"

#include "PuzzleWidget.generated.h"

class UImage;
class UTexture2D;

// 阶段5 Puzzle落地：小游戏UMG宿主，把Core层的Puzzle实例跑起来、把它的画布贴到一张Image上，
// 并把UE的键鼠事件转发进去。延续Source/Forever/UI/已有的Widget基类惯例（逻辑全在C++基类，
// Blueprint子类只摆布局），由AForeverPlayerController常驻持有一份实例，见
// UForeverStoryFrameworkComponent::ApplyStartPuzzle（StartPuzzleChange触发）。
UCLASS()
class FOREVER_API UPuzzleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 由UForeverStoryFrameworkComponent::ApplyStartPuzzle调用：创建对应类型的Core层Puzzle
	// 实例、Init()、把自己设为可见、切输入模式成UIOnly。UIOnly的原因跟SectionSpeaking一样
	// （见USectionSpeakingWidget.h类注释）：小游戏要通过NativeOnKeyDown接收键盘事件，UIOnly
	// 下Pawn的Enhanced Input收不到任何输入，不会跟移动/视角/MeetOption等操作打架。
	// @puzzleId: 小游戏类型标识（PuzzleFactory里注册的id，比如"tetris"）
	void StartPuzzle(const FString& puzzleId);

protected:
	virtual void NativeConstruct() override; // 默认Collapsed，没有小游戏在跑就不显示
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 命名避开跟Core层Canvas类同名，Blueprint子类里摆一个铺满的Image控件绑到这个名字上。
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> DisplayImage;

private:
	// 把canvas当前像素缓冲区贴到DisplayImage上——UTexture2D::CreateTransient+
	// Lock/Memcpy/Unlock/UpdateResource这套写法照抄老工程CanvasBuffer::ApplyImage
	// （E:\Projects\Forever_UE\Source\Forever\Utility\CanvasBuffer.cpp），不涉及文字，
	// 直接搬。
	void BlitCanvasToImage();

	// 小游戏结束（Loop返回非0）时调用：delete puzzle、隐藏自己、恢复正常的游戏内输入——
	// 跟UForeverStoryFrameworkComponent::ShowCurrentItem队列清空分支的收尾逻辑一致
	// （bShowMouseCursor=false+SetInputMode_GameOnly+SetFocusToGameViewport找回视口焦点）。
	void EndPuzzle();

	// 查真实视口像素分辨率，查不到时返回(fallbackWidth, fallbackHeight)——StartPuzzle()首次
	// 建画布、NativeTick()每帧比对是否发生了resize，共用这一份查询逻辑。
	static FIntPoint GetViewportSize(int32 fallbackWidth, int32 fallbackHeight);

	Canvas canvas; // 自己持有
	Puzzle* puzzle = nullptr; // 自己持有所有权，EndPuzzle()里delete

	UPROPERTY()
	TObjectPtr<UTexture2D> texture;

	// StartPuzzle()查不到真实视口分辨率时的兜底默认值（正常情况下不会用到）——不能再当成
	// "就是画布尺寸"来写死用了，见.cpp里StartPuzzle的实现：canvas.Init的实际参数是
	// GEngine->GameViewport->Viewport->GetSizeXY()查到的屏幕真实分辨率，DisplayImage是铺满
	// 整个屏幕的Image控件，画布尺寸跟屏幕分辨率不一致就会被拉伸变形。
	static constexpr int kFallbackCanvasWidth = 1280;
	static constexpr int kFallbackCanvasHeight = 720;
};
