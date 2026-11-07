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

// 游戏内地图：M键打开/Esc或M关闭(AForeverPlayerController::ToggleMap/HandleCloseMap)，
// 地图只光栅化一次，缩放/拖拽只改材质UV参数，详见MapWidget.md。
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
	// 打开时是UI Only输入，Enhanced Input收不到键，Esc/M要在这里直接拦截。
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

	// 把缩放/平移写进动态材质的UVScale/UVOffset参数(材质参数名MapTexture/UVScale/UVOffset，baseMapMaterial在Class Defaults指定)。
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

	// 把uvOffset的x/y各自clamp，保证拖拽/缩放后UV窗口不露出贴图外(范围公式见.cpp，cover裁边的轴范围更大)。
	void ClampUVOffset();

	// 每帧调用：查玩家当前世界位置+朝向，换算成地图纹理UV→当前缩放/平移下的屏幕UV→
	// Canvas Panel坐标(要叠加DisplayImage这次是居中摆放、不是贴着Panel左上角，见
	// ResizeDisplayImageToCoverViewport)，挪动PlayerMarker的Canvas Panel Slot位置+
	// 旋转角度。
	void UpdatePlayerMarker();

	// 每帧调用：把DisplayImage按地图真实宽高比改成刚好盖满视口的尺寸并居中(裁边的轴由ClampUVOffset放宽平移范围)；
	// 每帧重算以兼容运行时改分辨率。
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
