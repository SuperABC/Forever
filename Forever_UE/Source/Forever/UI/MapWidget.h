#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "player/canvas.h"

#include "MapWidget.generated.h"

class UImage;
class UButton;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;

// 游戏内地图：M键打开/Esc或M关闭(见AForeverPlayerController::ToggleMap/HandleCloseMap)，
// 画法照抄老工程——用Source/Dependence/player/canvas.h这个软件光栅画布(PhoneWidget/
// PuzzleWidget已经在用的同一个类)把地形/大路/园区/建筑画一次、贴进一张UTexture2D，缩放/
// 拖拽不重新光栅化，只改DisplayImage所贴材质的UVScale/UVOffset参数(和老工程MapMaterial
// 同一套约定)。
//
// 跟PhoneWidget/PuzzleWidget的关键差异：那两个每次resize都要重新画(canvas尺寸=屏幕像素
// 尺寸，画面内容本身每帧在变)；这里的canvas尺寸是按地图网格大小×超采样倍数算出来的固定
// 尺寸，跟屏幕分辨率无关，地图生成后内容不会变，只在第一次OpenMap时画一次
// (DrawFullMap()+BlitCanvasToImage())，NativeTick不重新光栅化，只做材质参数/玩家朝向
// 标记位置的轻量更新。
UCLASS()
class FOREVER_API UMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 由AForeverPlayerController::ToggleMap调用(M键)：可见就CloseMap，不可见就OpenMap。
	void ToggleMap();

	// 由AForeverPlayerController::HandleCloseMap调用(Esc键)：地图已经关着时什么都不做，
	// 不会把它打开——跟ToggleMap的区别就在这里，Esc只负责关。
	void CloseMap();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;
	// 地图打开时用的是UI Only输入模式，所有键盘输入会优先交给当前有键盘焦点的widget
	// (OpenMap()里SetKeyboardFocus()设成了这个widget自己)，不会走到
	// AForeverPlayerController::SetupInputComponent绑的Enhanced Input动作那条路——
	// Esc/M要在这里直接拦截，不能指望Controller那边的ToggleMap/CloseMap绑定在地图打开期间
	// 还能生效。
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 静态地图(地形/大路/园区/建筑)，贴dynamicMapMaterial(不是直接贴texture——缩放/拖拽靠
	// 材质UV参数，不是重新画)。
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> DisplayImage;

	// 玩家位置朝向标记(小箭头)，摆在跟DisplayImage同一个Canvas Panel里，每帧跟随玩家
	// 移动/转向重新摆放，见UpdatePlayerMarker()。
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> PlayerMarker;

	// 备用关闭按钮，Esc/M已经能关，这个给鼠标用户多一个选择，蓝图里可以不绑事件(未绑定
	// 不影响Esc/M照常工作)。
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	UFUNCTION()
	void HandleCloseButtonClicked();

	// 老工程MapMaterial同一套参数命名(MapTexture/UVScale/UVOffset)，Editor里建好之后在
	// Class Defaults里指定。
	UPROPERTY(EditDefaultsOnly, Category = "Map")
	TObjectPtr<UMaterialInterface> baseMapMaterial;

private:
	void OpenMap();

	// 第一次OpenMap时跑一遍：按Map::GetSize()×超采样倍数定canvas尺寸，依次画地形/大路/
	// 园区/建筑，最后BlitCanvasToImage()贴进texture。地图生成后不会变，只跑这一次。
	void DrawFullMap();

	// 照抄PhoneWidget::BlitCanvasToImage的套路：CreateTransient+Lock/Memcpy/Unlock+
	// UpdateResource，贴好之后换成dynamicMapMaterial的MapTexture纹理参数(不是直接
	// SetBrushFromTexture，这张图要经过材质的UV变换)。
	void BlitCanvasToImage();

	// 按当前zoomLevel/uvOffset更新dynamicMapMaterial的UVScale/UVOffset两个标量/矢量
	// 参数。
	void ApplyZoomPanToMaterial();

	// 把uvOffset的x/y各自clamp到±(0.5-0.5/zoomLevel)范围内——跟老工程ApplyTransform同一个
	// 公式，保证拖拽/缩放后UV窗口始终落在[0,1]贴图范围内，不会露出贴图外的区域。
	void ClampUVOffset();

	// 每帧调用：查玩家当前世界位置+朝向，换算成地图纹理UV→当前缩放/平移下的屏幕UV→
	// Canvas Panel坐标(要叠加DisplayImage这次是居中摆放、不是贴着Panel左上角，见
	// ResizeDisplayImageToCoverViewport)，挪动PlayerMarker的Canvas Panel Slot位置+
	// 旋转角度。
	void UpdatePlayerMarker();

	// 每帧调用：把DisplayImage的Canvas Panel Slot改成"按地图真实宽高比(mapGridWidth:
	// mapGridHeight，不是硬编码的1:1)、刚好盖满整个视口不留空白"的尺寸、居中摆放——地图
	// 不是正方形时(比如2:1)这个尺寸也会是2:1的矩形，不会被拉伸成方的。长宽比不匹配的那条
	// 轴会裁掉一部分到屏幕外，但ClampUVOffset()按同一个尺寸放宽了可平移范围，拖拽依然能
	// 摸到被裁掉的那部分(不是永久看不到，只是初始状态看不到，这是用户明确要的效果——铺满
	// 不留空白，靠拖拽去看边缘内容)。每帧重算是为了兼容窗口运行时改变分辨率的情况，开销
	// 很小。
	void ResizeDisplayImageToCoverViewport();

	// ResizeDisplayImageToCoverViewport/UpdatePlayerMarker/ClampUVOffset共用的"cover
	// 缩放"计算——取viewportSize.X/mapGridWidth和viewportSize.Y/mapGridHeight两个缩放
	// 系数里更大的那个，保证按地图原始宽高比放大后两个方向都至少盖满视口，返回
	// DisplayImage应该有的像素尺寸。
	FVector2D ComputeCoverSize(const FVector2D& viewportSize) const;

	Canvas canvas;

	// Map::GetSize()的宽高(格数)，DrawFullMap()里查一次缓存下来——ResizeDisplayImageToCoverViewport/
	// UpdatePlayerMarker都要按地图真实宽高比算缩放，不能硬编码假设地图是正方形。地图生成前
	// (bMapDrawn==false)默认1:1，不会被用到(两个函数都在地图画完、Visible之后才跑)。
	int mapGridWidth = 1;
	int mapGridHeight = 1;

	UPROPERTY()
	TObjectPtr<UTexture2D> texture;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> dynamicMapMaterial;

	bool bMapDrawn = false;

	float zoomLevel = 1.f;
	FVector2D uvOffset = FVector2D::ZeroVector;

	bool bDragging = false;
	FVector2D lastDragScreenPos = FVector2D::ZeroVector;

	static constexpr float kMinZoom = 1.f;
	static constexpr float kMaxZoom = 10.f;
	static constexpr float kZoomStep = 0.15f;

	// 超采样上限——canvas像素尺寸=地图网格尺寸(格数)×倍数，倍数按这个上限夹在
	// [1, kMaxSuperSample]之间，避免地图特别大时画布占用内存过多。
	static constexpr int kTargetResolution = 2048;
	static constexpr int kMaxSuperSample = 8;
};
