#include "UI/PuzzleWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "UI/CanvasInputTranslation.h"
#include "Framework/ForeverFrameworkActor.h"
#include "common/implement.h"
#include "common/registry.h"

#undef UpdateResource


using namespace std;

// 现场构造一个PostImplement供PuzzleMod::Init/Loop向Core查询(比如wxdj查招式数据文件路径)——
// 很轻量(7个裸指针+一个JsonValue缓存)，不用持久持有；做法和UPhoneWidget::NativeTick一致。
// 找不到AForeverFrameworkActor(理论上不会发生)时对应域指针传nullptr，PostImplement::Post
// 自己会处理成查询失败。
static PostImplement MakePost(UWorld* world) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(world, AForeverFrameworkActor::StaticClass()));
	return PostImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		framework ? framework->GetStory() : nullptr,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);
}

void UPuzzleWidget::NativeConstruct() {
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);

	// UUserWidget::NativeSupportsKeyboardFocus()默认返回bIsFocusable(默认false)——不像
	// SectionSpeaking那样树里有个真实的Button(点一次就能拿到焦点，之后键盘事件靠这个Button
	// 再往上冒泡到USectionSpeakingWidget::NativeOnKeyDown)，PuzzleWidget树里只有一张纯展示
	// 用的Image，没有任何天生可获得焦点的子控件，不显式打开这个开关的话
	// FSlateApplication::SetKeyboardFocus会直接判定"这个控件不支持键盘焦点"而静默失败，
	// 表现为"键盘完全没反应"（鼠标点击本身没问题——这次的Tetris只读键盘，没有接鼠标操作，
	// 点了没反应是预期行为，不是bug）。
	SetIsFocusable(true);
}

void UPuzzleWidget::StartPuzzle(const FString& puzzleId, const FString& param) {
	if (puzzle) {
		delete puzzle;
		puzzle = nullptr;
	}

	// DisplayImage铺满整个屏幕，画布尺寸必须跟屏幕真实分辨率一致，不然贴图时就会被拉伸/
	// 压扁变形——GetViewportSize查的是真实像素分辨率(不受UMG的DPI缩放影响，跟UMG自己那种
	// "Slate单位"的GetViewportSize不是一回事)。
	FIntPoint canvasSize = GetViewportSize(kFallbackCanvasWidth, kFallbackCanvasHeight);
	canvas.Init(canvasSize.X, canvasSize.Y);
	puzzle = new Puzzle(&Registry::Get().GetPuzzleFactory(), string(TCHAR_TO_UTF8(*puzzleId)));
	puzzle->SetParam(string(TCHAR_TO_UTF8(*param))); // 必须在Init之前，让Init里就能用上
	PostImplement post = MakePost(GetWorld());
	puzzle->Init(&canvas, &post);

	SetVisibility(ESlateVisibility::Visible);

	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = true;
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(playerController, this, EMouseLockMode::DoNotLock, false);
		// SetInputMode_UIOnlyEx内部也会尝试把焦点给到WidgetToFocus，这里再显式抢一次
		// 双重保险——现在SetIsFocusable(true)已经在NativeConstruct里打开，这一步能成功拿到
		// 焦点。
		SetKeyboardFocus();
	}

	// 冻结城市模拟，跟UForeverBuildingFrameworkComponent::RequestFreezeUntilLodSettled同一个
	// 手法。UMG的NativeTick走Slate自己的每帧真实时间(FApp::GetDeltaTime)，不经过
	// AWorldSettings::TimeDilation缩放，所以世界归零之后这里的puzzle->Loop还是能按正常速度
	// 跑，只是场景里的Actor/AI/物理(包括AForeverFrameworkActor::Tick驱动的城市模拟)会停住。
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.f);
}

void UPuzzleWidget::EndPuzzle() {
	delete puzzle;
	puzzle = nullptr;

	SetVisibility(ESlateVisibility::Collapsed);

	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = false;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(playerController);
		UWidgetBlueprintLibrary::SetFocusToGameViewport();
	}

	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.f);
}

void UPuzzleWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime) {
	Super::NativeTick(MyGeometry, DeltaTime);

	if (!puzzle) return;

	// 每帧比对一次真实视口分辨率——用户拖动窗口/切换分辨率时会在这里被发现，canvas.Resize
	// 内部会在尺寸真的变了的情况下才重新分配缓冲区并调用PuzzleMod注册的ResizeFunc(如果有)，
	// 让它有机会重新计算依赖画布尺寸的内部布局(比如TetrisPuzzle的格子大小/棋盘偏移)。
	FIntPoint viewportSize = GetViewportSize(canvas.GetWidth(), canvas.GetHeight());
	canvas.Resize(viewportSize.X, viewportSize.Y);

	PostImplement post = MakePost(GetWorld());
	int result = puzzle->Loop(&canvas, FMath::RoundToInt(DeltaTime * 1000.f), &post);
	BlitCanvasToImage();

	if (result != 0) {
		EndPuzzle();
	}
}

void UPuzzleWidget::BlitCanvasToImage() {
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
	if (DisplayImage) DisplayImage->SetBrushFromTexture(texture, true);
}

FReply UPuzzleWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) {
	int32 code = CanvasInputTranslation::TranslateKey(InKeyEvent.GetKey());
	if (code != 0) canvas.PushKey(code);
	return FReply::Handled();
}

FReply UPuzzleWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) {
	int32 code = CanvasInputTranslation::TranslateKey(InKeyEvent.GetKey());
	if (code != 0) canvas.PushKey(code | KEY_RELEASED_FLAG);
	return FReply::Handled();
}

// 鼠标事件的Slate本地坐标(受DPI缩放影响，范围是控件的Slate尺寸)换算成画布像素坐标——画布
// 尺寸是视口真实像素，两者只要DPI缩放不是1就不相等，不换算会以左上角为原点按比例错位。
void UPuzzleWidget::UpdateMousePos(const FGeometry& geometry, const FPointerEvent& mouseEvent) {
	FVector2D localPos = geometry.AbsoluteToLocal(mouseEvent.GetScreenSpacePosition());
	FVector2D localSize = geometry.GetLocalSize();
	if (localSize.X > 0 && localSize.Y > 0) {
		localPos.X *= canvas.GetWidth() / localSize.X;
		localPos.Y *= canvas.GetHeight() / localSize.Y;
	}
	canvas.SetMousePos(FMath::RoundToInt(localPos.X), FMath::RoundToInt(localPos.Y));
}

FReply UPuzzleWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	UpdateMousePos(InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UPuzzleWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	int32 button = CanvasInputTranslation::TranslateMouseButton(InMouseEvent.GetEffectingButton());
	UpdateMousePos(InGeometry, InMouseEvent); // 按键事件自带位置，不依赖之前有没有移动过
	if (button >= 0) canvas.PushMouseButton(button, true);
	return FReply::Handled();
}

FReply UPuzzleWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	int32 button = CanvasInputTranslation::TranslateMouseButton(InMouseEvent.GetEffectingButton());
	UpdateMousePos(InGeometry, InMouseEvent); // 按键事件自带位置，不依赖之前有没有移动过
	if (button >= 0) canvas.PushMouseButton(button, false);
	return FReply::Handled();
}

FReply UPuzzleWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	// 滚轮事件自带当前位置，先同步一次，免得小游戏取滚轮时用的是过期的鼠标位置。
	UpdateMousePos(InGeometry, InMouseEvent);
	// GetWheelDelta()一格通常是+-1.0，转成整数刻度(向上为正)交给Canvas累加，小游戏每帧取走。
	canvas.PushMouseWheel(FMath::RoundToInt(InMouseEvent.GetWheelDelta()));
	return FReply::Handled();
}

FIntPoint UPuzzleWidget::GetViewportSize(int32 fallbackWidth, int32 fallbackHeight) {
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport) {
		FIntPoint viewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
		if (viewportSize.X > 0 && viewportSize.Y > 0) return viewportSize;
	}
	return FIntPoint(fallbackWidth, fallbackHeight);
}
