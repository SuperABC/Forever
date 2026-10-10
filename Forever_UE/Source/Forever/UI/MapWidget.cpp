#include "UI/MapWidget.h"

#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

#include "Framework/ForeverFrameworkActor.h"
#include "map/map.h"
#include "map/zone.h"
#include "map/building.h"
#include "map/geometry.h"

#include <cmath>
#include <unordered_map>

#undef UpdateResource


namespace {

	// FNV-1a字符串哈希——用来把地形type/园区建筑name映射成固定的色相，同一个名字不管重启
	// 多少次都是同一个颜色，比老工程"每次运行重新随机"更稳定、也不会采到过暗/过灰的颜色
	// (饱和度/明度固定，只有色相随哈希变化)，见MapWidget.md"颜色方案"一节。
	uint32 HashStringFNV1a(const std::string& s) {
		uint32 h = 2166136261u;
		for (char c : s) {
			h ^= static_cast<uint8>(c);
			h *= 16777619u;
		}
		return h;
	}

	Color HSVtoRGB(float hue, float saturation, float value) {
		float c = value * saturation;
		float x = c * (1.f - std::fabs(std::fmod(hue / 60.f, 2.f) - 1.f));
		float m = value - c;
		float r = 0.f, g = 0.f, b = 0.f;
		if (hue < 60.f) { r = c; g = x; b = 0.f; }
		else if (hue < 120.f) { r = x; g = c; b = 0.f; }
		else if (hue < 180.f) { r = 0.f; g = c; b = x; }
		else if (hue < 240.f) { r = 0.f; g = x; b = c; }
		else if (hue < 300.f) { r = x; g = 0.f; b = c; }
		else { r = c; g = 0.f; b = x; }
		return Color{
			static_cast<uint8_t>((r + m) * 255.f),
			static_cast<uint8_t>((g + m) * 255.f),
			static_cast<uint8_t>((b + m) * 255.f),
			255 };
	}

	Color ColorForName(const std::string& name, std::unordered_map<std::string, Color>& cache) {
		auto it = cache.find(name);
		if (it != cache.end()) {
			return it->second;
		}

		// 直接用hash%360做色相的话，名字数量一少(比如就"平原"/"山地"/"海洋"几种地形)，
		// 哈希值完全可能恰好落在相邻的色相附近(比如40°和55°)，饱和度/明度又是固定常量，
		// 视觉上几乎分不出来。改成离散分桶：先把色相量化成12个均匀分布、彼此间隔30°的桶
		// (用哈希的一段选桶号)，桶内只允许一个很小范围的抖动(不会抖到隔壁桶的范围里)；
		// 再用哈希另外两段分别在饱和度、明度上各做一次二选一的跳变——相当于12色相桶×2
		// 饱和度×2明度=48种组合的离散调色板，"哈希值差一点"的两个名字很大概率落进完全不同
		// 的桶组合，不会再像纯连续映射那样挨得很近。同样是纯按名字算的确定性结果，不依赖
		// 遍历顺序，同名字跨session还是同一个颜色。
		// 固定异或盐值整体平移一下哈希空间——纯粹为了把"plain"这个具体名字挪出蓝色那个桶
		// (跟玩家朝向箭头的颜色撞了)，不影响前面说的分桶/跳变逻辑，只是把所有名字的取值
		// 起点统一挪了一下，各名字之间原有的区分度不受影响。
		uint32 h = HashStringFNV1a(name) ^ 0x9E3779B9u;
		int hueBin = h % 12;
		float hueJitter = static_cast<float>((h / 12) % 100) / 100.f * 20.f; // 0~20°，不超出30°一格的范围
		float hue = hueBin * 30.f + hueJitter;
		float saturation = ((h / 1200) % 2 == 0) ? 0.55f : 0.85f;
		float value = ((h / 2400) % 2 == 0) ? 0.75f : 0.95f;

		Color color = HSVtoRGB(hue, saturation, value);
		cache[name] = color;
		return color;
	}

	Color Darken(Color c, float factor) {
		return Color{
			static_cast<uint8_t>(c.r * factor),
			static_cast<uint8_t>(c.g * factor),
			static_cast<uint8_t>(c.b * factor),
			c.a };
	}

	// 园区/建筑footprint——Zone/Building都是Quad(中心+尺寸)带自己的GetRotation()(弧度)，
	// 不是轴对齐矩形，不能用Canvas::PutRect(那个只能画平行坐标轴的矩形)，照抄老工程"4个
	// 角点拆成2个三角形"的画法，再叠一层边框线区分层次(填色之上画4条深一点的边)。
	void DrawQuadFootprint(Canvas& canvas, float centerX, float centerY, float sizeX, float sizeY,
		float rotationRadians, int scale, Color fillColor) {
		float hx = sizeX * 0.5f, hy = sizeY * 0.5f;
		float cosT = std::cos(rotationRadians), sinT = std::sin(rotationRadians);
		const FVector2D local[4] = { {-hx,-hy}, {hx,-hy}, {hx,hy}, {-hx,hy} };
		FIntPoint pts[4];
		for (int i = 0; i < 4; i++) {
			float wx = centerX + local[i].X * cosT - local[i].Y * sinT;
			float wy = centerY + local[i].X * sinT + local[i].Y * cosT;
			pts[i] = FIntPoint(FMath::RoundToInt(wx * scale), FMath::RoundToInt(wy * scale));
		}

		canvas.SetColor(fillColor.r, fillColor.g, fillColor.b);
		canvas.PutTriangle(pts[0].X, pts[0].Y, pts[1].X, pts[1].Y, pts[2].X, pts[2].Y, true);
		canvas.PutTriangle(pts[0].X, pts[0].Y, pts[2].X, pts[2].Y, pts[3].X, pts[3].Y, true);

		Color borderColor = Darken(fillColor, 0.6f);
		canvas.SetColor(borderColor.r, borderColor.g, borderColor.b);
		for (int i = 0; i < 4; i++) {
			int j = (i + 1) % 4;
			canvas.PutLine(pts[i].X, pts[i].Y, pts[j].X, pts[j].Y);
		}
	}

} // namespace

void UMapWidget::NativeConstruct() {
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
	SetIsFocusable(true);

	if (CloseButton) {
		CloseButton->OnClicked.AddDynamic(this, &UMapWidget::HandleCloseButtonClicked);
	}
}

void UMapWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime) {
	Super::NativeTick(MyGeometry, DeltaTime);

	if (GetVisibility() != ESlateVisibility::Visible) return;
	ResizeDisplayImageToCoverViewport();
	UpdatePlayerMarker();
}

FReply UMapWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) {
	const FKey key = InKeyEvent.GetKey();
	if (key == EKeys::Escape || key == EKeys::M) {
		CloseMap();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void UMapWidget::ToggleMap() {
	if (GetVisibility() == ESlateVisibility::Visible) CloseMap();
	else OpenMap();
}

void UMapWidget::OpenMap() {
	if (!bMapDrawn) {
		DrawFullMap();
		BlitCanvasToImage();
		bMapDrawn = true;

		if (DisplayImage && baseMapMaterial) {
			dynamicMapMaterial = UMaterialInstanceDynamic::Create(baseMapMaterial, this);
			if (dynamicMapMaterial) {
				if (texture) {
					dynamicMapMaterial->SetTextureParameterValue(TEXT("MapTexture"), texture);
				}
				DisplayImage->SetBrushFromMaterial(dynamicMapMaterial);
			}
		}
		ApplyZoomPanToMaterial();
	}

	SetVisibility(ESlateVisibility::Visible);

	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = true;
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(playerController, this, EMouseLockMode::DoNotLock, false);
		SetKeyboardFocus();
	}
}

void UMapWidget::CloseMap() {
	if (GetVisibility() != ESlateVisibility::Visible) return;

	SetVisibility(ESlateVisibility::Collapsed);
	bDragging = false;

	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = false;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(playerController);
		UWidgetBlueprintLibrary::SetFocusToGameViewport();
	}
}

void UMapWidget::HandleCloseButtonClicked() {
	CloseMap();
}

void UMapWidget::DrawFullMap() {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	Map* map = framework ? framework->GetMap() : nullptr;
	if (!map) return;

	auto size = map->GetSize();
	int mapWidth = size.first, mapHeight = size.second;
	if (mapWidth <= 0 || mapHeight <= 0) return;

	mapGridWidth = mapWidth;
	mapGridHeight = mapHeight;

	int scale = FMath::Clamp(kTargetResolution / FMath::Max(mapWidth, mapHeight), 1, kMaxSuperSample);
	canvas.Init(mapWidth * scale, mapHeight * scale);

	// 地形：按格填块，PutRect只用在这里——地形格子是轴对齐规则网格，没有旋转角，PutRect
	// 填一个倍数×倍数的色块代替老工程的单像素PutPixel，画布分辨率更高、边界更清晰。
	std::unordered_map<std::string, Color> terrainColors;
	for (int y = 0; y < mapHeight; y++) {
		for (int x = 0; x < mapWidth; x++) {
			Color color = ColorForName(map->GetTerrain(x, y), terrainColors);
			canvas.SetColor(color.r, color.g, color.b);
			canvas.PutRect(x * scale, y * scale, (x + 1) * scale - 1, (y + 1) * scale - 1, true);
		}
	}

	// 大路：Map::GetRoads()只装大路，不含小路/人行道(GetPathRoads()是另一批，这次按要求
	// 不画)。沿曲线采样几个点、分段画等宽线，贴合Road::GetTotalWidth()的真实宽度——用
	// Canvas::PutLine(...,width)这个新加的粗线primitive(内部是填充矩形，不是多条1像素
	// 线堆叠)，斜线方向不会再有缝隙。
	for (const Road* road : map->GetRoads()) {
		if (!road) continue;
		int widthPixels = FMath::Max(1, FMath::RoundToInt(road->GetTotalWidth() * scale));
		constexpr int kSamples = 8;
		Node prev = road->GetPoint(0.f);
		for (int i = 1; i <= kSamples; i++) {
			float f = static_cast<float>(i) / kSamples;
			Node cur = road->GetPoint(f);
			canvas.PutLine(
				FMath::RoundToInt(prev.GetX() * scale), FMath::RoundToInt(prev.GetY() * scale),
				FMath::RoundToInt(cur.GetX() * scale), FMath::RoundToInt(cur.GetY() * scale),
				widthPixels);
			prev = cur;
		}
	}

	// 园区先画、建筑后画——有父园区的建筑footprint严格落在父园区footprint内部，后画的
	// 建筑自然叠在园区上面，不需要额外判断"园区内/外建筑"。
	std::unordered_map<std::string, Color> zoneColors;
	for (const auto& [name, zone] : map->GetZones()) {
		if (!zone) continue;
		Color color = ColorForName(zone->GetName(), zoneColors);
		DrawQuadFootprint(canvas, zone->GetPosX(), zone->GetPosY(), zone->GetSizeX(), zone->GetSizeY(),
			zone->GetRotation(), scale, color);
	}

	std::unordered_map<std::string, Color> buildingColors;
	for (const auto& [name, building] : map->GetBuildings()) {
		if (!building) continue;
		Color color = ColorForName(building->GetName(), buildingColors);
		DrawQuadFootprint(canvas, building->GetPosX(), building->GetPosY(), building->GetSizeX(), building->GetSizeY(),
			building->GetRotation(), scale, color);
	}
}

void UMapWidget::BlitCanvasToImage() {
	int width = canvas.GetWidth(), height = canvas.GetHeight();
	if (width <= 0 || height <= 0) return;

	if (!texture || texture->GetSizeX() != width || texture->GetSizeY() != height) {
		texture = UTexture2D::CreateTransient(width, height);
		if (!texture) return;
	}

	void* data = texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(data, canvas.GetData(), canvas.GetDataSize());
	texture->GetPlatformData()->Mips[0].BulkData.Unlock();

	texture->UpdateResource();
}

void UMapWidget::ApplyZoomPanToMaterial() {
	if (!dynamicMapMaterial) return;
	dynamicMapMaterial->SetScalarParameterValue(TEXT("UVScale"), 1.f / zoomLevel);
	dynamicMapMaterial->SetVectorParameterValue(TEXT("UVOffset"), FLinearColor(uvOffset.X, uvOffset.Y, 0.f, 0.f));
}

void UMapWidget::ClampUVOffset() {
	// 老公式halfRange=0.5-0.5/zoomLevel只在"DisplayImage正好贴合视口、没有任何内容被
	// cover裁到屏幕外"时才对(等价于下面公式里k=1的特例)。cover铺满会裁边的那条轴，
	// DisplayImage本身比视口大，光靠这个老公式会把uvOffset锁在0附近，永远摸不到被裁掉
	// 那部分内容(真实复现过：玩家走到离北边界50地图单位时箭头就跑出画面，因为那部分
	// 内容本来就在屏幕外、而且clamp不让拖过去)。
	//
	// 推导：材质公式textureUV=(screenUV-0.5)*UVScale+0.5+uvOffset，screenUV在
	// DisplayImage整个尺寸上取[0,1]，但视口只看得到其中一段：
	// screenUV_visible=[0.5-k/2, 0.5+k/2]，k=viewportSize/displaySize(这条轴视口占
	// DisplayImage实际尺寸的比例，cover裁边的轴k<1，贴合视口的轴k=1)。代入材质公式、
	// 令可见textureUV范围贴着[0,1]两端(刚好能摸到边界、又不会漏出贴图外)，解出
	// uvOffset的合法范围是±(0.5-k/(2*zoomLevel))——k=1时退化成老公式，k<1时range
	// 变大，允许拖到更靠边的位置，正好补上被cover裁掉的那部分。
	FVector2D viewportSize = GetCachedGeometry().GetLocalSize();
	FVector2D displaySize = ComputeCoverSize(viewportSize);
	if (displaySize.X <= 0.f || displaySize.Y <= 0.f) return;

	float kx = viewportSize.X / displaySize.X;
	float ky = viewportSize.Y / displaySize.Y;

	float halfRangeX = 0.5f - kx / (2.f * zoomLevel);
	float halfRangeY = 0.5f - ky / (2.f * zoomLevel);

	uvOffset.X = FMath::Clamp(uvOffset.X, -halfRangeX, halfRangeX);
	uvOffset.Y = FMath::Clamp(uvOffset.Y, -halfRangeY, halfRangeY);
}

FReply UMapWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	if (!DisplayImage) return FReply::Handled();

	// 以鼠标当前位置为缩放中心(照抄老工程的行为，不是永远缩放视口正中心)：先算出缩放前
	// 鼠标指着的是贴图上哪个点(textureUV)，再反解缩放后应该有的uvOffset，让这个点缩放
	// 前后始终停在鼠标指针下面不动。推导：材质公式textureUV=(screenUV-0.5)*(1/zoom)+0.5+
	// uvOffset，要求同一个textureUV在新旧zoom下对应同一个screenUV，解出
	// uvOffset_new = uvOffset_old + (screenUV-0.5)*(1/zoomOld-1/zoomNew)。
	FGeometry displayGeometry = DisplayImage->GetCachedGeometry();
	FVector2D localPos = displayGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	FVector2D localSize = displayGeometry.GetLocalSize();
	if (localSize.X <= 0.f || localSize.Y <= 0.f) return FReply::Handled();

	FVector2D screenUV = localPos / localSize;

	float oldZoom = zoomLevel;
	float newZoom = FMath::Clamp(zoomLevel + InMouseEvent.GetWheelDelta() * kZoomStep * zoomLevel, kMinZoom, kMaxZoom);

	uvOffset += (screenUV - FVector2D(0.5f, 0.5f)) * (1.f / oldZoom - 1.f / newZoom);
	zoomLevel = newZoom;

	ClampUVOffset();
	ApplyZoomPanToMaterial();
	return FReply::Handled();
}

FReply UMapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) {
		bDragging = true;
		lastDragScreenPos = InMouseEvent.GetScreenSpacePosition();
	}
	return FReply::Handled();
}

FReply UMapWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) {
		bDragging = false;
	}
	return FReply::Handled();
}

FReply UMapWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	if (bDragging && DisplayImage) {
		FVector2D currentScreenPos = InMouseEvent.GetScreenSpacePosition();
		FVector2D deltaScreen = currentScreenPos - lastDragScreenPos;
		lastDragScreenPos = currentScreenPos;

		FVector2D localSize = DisplayImage->GetCachedGeometry().GetLocalSize();
		if (localSize.X > 0.f && localSize.Y > 0.f) {
			// 屏幕像素位移->DisplayImage本地尺寸占比->按当前zoomLevel换算成贴图UV位移，
			// 缩放越大同样的屏幕拖动距离对应越小的地图距离，手感在任何缩放级别下一致。
			uvOffset.X -= (deltaScreen.X / localSize.X) / zoomLevel;
			uvOffset.Y -= (deltaScreen.Y / localSize.Y) / zoomLevel;
			ClampUVOffset();
			ApplyZoomPanToMaterial();
		}
	}
	return FReply::Handled();
}

FVector2D UMapWidget::ComputeCoverSize(const FVector2D& viewportSize) const {
	// "cover"缩放：按地图真实宽高比(mapGridWidth:mapGridHeight)放大，取
	// viewportSize.X/mapGridWidth和viewportSize.Y/mapGridHeight两个缩放系数里更大的那个，
	// 保证放大后两个方向都至少盖满视口，不留空白——铺不满的那条轴会裁掉一部分到屏幕外，
	// 但ClampUVOffset()按这个尺寸同步放宽了可平移范围，拖拽依然能摸到被裁掉的那部分内容
	// (不是"裁了就永远看不到"，只是初始zoom=1、uvOffset=0时刚好看不到，拖一下就能看到)。
	float coverScale = FMath::Max(
		viewportSize.X / FMath::Max(mapGridWidth, 1),
		viewportSize.Y / FMath::Max(mapGridHeight, 1));
	return FVector2D(mapGridWidth * coverScale, mapGridHeight * coverScale);
}

void UMapWidget::ResizeDisplayImageToCoverViewport() {
	if (!DisplayImage) return;
	UCanvasPanelSlot* slot = Cast<UCanvasPanelSlot>(DisplayImage->Slot);
	if (!slot) return;

	FVector2D viewportSize = GetCachedGeometry().GetLocalSize();
	if (viewportSize.X <= 0.f || viewportSize.Y <= 0.f) return;

	FVector2D displaySize = ComputeCoverSize(viewportSize);

	// 单点锚在Canvas Panel正中心+自身也按中心对齐+零偏移——这块矩形的中心永远在Panel
	// 中心，长宽比不匹配的那条轴上两侧会留一点空白(不会拉伸变形，也不会裁掉任何地图
	// 内容)。
	slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	slot->SetAlignment(FVector2D(0.5f, 0.5f));
	slot->SetPosition(FVector2D::ZeroVector);
	slot->SetSize(displaySize);
}

void UMapWidget::UpdatePlayerMarker() {
	if (!PlayerMarker || !DisplayImage) return;

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	Map* map = framework ? framework->GetMap() : nullptr;
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!map || !pawn) return;

	auto size = map->GetSize();
	if (size.first <= 0 || size.second <= 0) return;

	FVector location = pawn->GetActorLocation() / 1000.f; // UE单位->地图单位(1地图格=1000UE单位)
	FVector2D textureUV(location.X / size.first, location.Y / size.second);

	// 跟材质里"(screenUV-0.5)*UVScale+0.5+UVOffset=textureUV"这条正变换反过来解screenUV，
	// UVScale=1/zoomLevel，所以screenUV=(textureUV-0.5-UVOffset)*zoomLevel+0.5。
	FVector2D screenUV = (textureUV - FVector2D(0.5f, 0.5f) - uvOffset) * zoomLevel + FVector2D(0.5f, 0.5f);

	// PlayerMarker是Canvas Panel的平级子节点，不是DisplayImage的子节点——它的Slot Position
	// 是相对Canvas Panel原点算的，不是相对DisplayImage。DisplayImage现在是居中摆放的矩形
	// (ResizeDisplayImageToCoverViewport，尺寸按地图真实宽高比算，不是硬编码正方形)，不是
	// 贴着Panel左上角，所以这里要自己算一遍DisplayImage左上角相对Panel的偏移，再加上
	// screenUV×这个矩形的实际尺寸，不能直接用DisplayImage的本地坐标系，也不能假设是正方形。
	FVector2D viewportSize = GetCachedGeometry().GetLocalSize();
	FVector2D displaySize = ComputeCoverSize(viewportSize);
	FVector2D displayTopLeft((viewportSize.X - displaySize.X) * 0.5f, (viewportSize.Y - displaySize.Y) * 0.5f);
	FVector2D markerPos = displayTopLeft + screenUV * displaySize;

	if (UCanvasPanelSlot* slot = Cast<UCanvasPanelSlot>(PlayerMarker->Slot)) {
		// Alignment默认(0,0)的话SetPosition摆的是widget左上角，不是中心——这里强制设成
		// (0.5,0.5)，让SetPosition摆的坐标对应widget自己的中心点，箭头图标的中心才会真正
		// 落在玩家所在的那个点上，不是某个角落。
		slot->SetAlignment(FVector2D(0.5f, 0.5f));
		slot->SetPosition(markerPos);
	}

	// 朝向：世界Yaw和Slate屏幕角度的符号/零点约定不一定一致，实现完之后要现场核对箭头转的
	// 方向是否和角色实际朝向一致，不对的话在这里加个负号/90度偏移修正，见MapWidget.md。
	PlayerMarker->SetRenderTransformAngle(pawn->GetActorRotation().Yaw);
}
