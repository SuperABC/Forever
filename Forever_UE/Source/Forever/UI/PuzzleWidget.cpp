#include "UI/PuzzleWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "common/registry.h"

#undef UpdateResource


using namespace std;

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

void UPuzzleWidget::StartPuzzle(const FString& puzzleId) {
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
	puzzle->Init(&canvas, nullptr);

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

	int result = puzzle->Loop(&canvas, FMath::RoundToInt(DeltaTime * 1000.f), nullptr);
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
	int32 code = TranslateKey(InKeyEvent.GetKey());
	if (code != 0) canvas.PushKey(code);
	return FReply::Handled();
}

FReply UPuzzleWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) {
	int32 code = TranslateKey(InKeyEvent.GetKey());
	if (code != 0) canvas.PushKey(code | KEY_RELEASED_FLAG);
	return FReply::Handled();
}

FReply UPuzzleWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	FVector2D localPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	canvas.SetMousePos(FMath::RoundToInt(localPos.X), FMath::RoundToInt(localPos.Y));
	return FReply::Handled();
}

FReply UPuzzleWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	int32 button = TranslateMouseButton(InMouseEvent.GetEffectingButton());
	if (button >= 0) canvas.PushMouseButton(button, true);
	return FReply::Handled();
}

FReply UPuzzleWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	int32 button = TranslateMouseButton(InMouseEvent.GetEffectingButton());
	if (button >= 0) canvas.PushMouseButton(button, false);
	return FReply::Handled();
}

int32 UPuzzleWidget::TranslateKey(const FKey& key) {
	static const TMap<FKey, int32> table = {
		{ EKeys::A, 'a' }, { EKeys::B, 'b' }, { EKeys::C, 'c' }, { EKeys::D, 'd' }, { EKeys::E, 'e' },
		{ EKeys::F, 'f' }, { EKeys::G, 'g' }, { EKeys::H, 'h' }, { EKeys::I, 'i' }, { EKeys::J, 'j' },
		{ EKeys::K, 'k' }, { EKeys::L, 'l' }, { EKeys::M, 'm' }, { EKeys::N, 'n' }, { EKeys::O, 'o' },
		{ EKeys::P, 'p' }, { EKeys::Q, 'q' }, { EKeys::R, 'r' }, { EKeys::S, 's' }, { EKeys::T, 't' },
		{ EKeys::U, 'u' }, { EKeys::V, 'v' }, { EKeys::W, 'w' }, { EKeys::X, 'x' }, { EKeys::Y, 'y' },
		{ EKeys::Z, 'z' },
		{ EKeys::Zero, '0' }, { EKeys::One, '1' }, { EKeys::Two, '2' }, { EKeys::Three, '3' },
		{ EKeys::Four, '4' }, { EKeys::Five, '5' }, { EKeys::Six, '6' }, { EKeys::Seven, '7' },
		{ EKeys::Eight, '8' }, { EKeys::Nine, '9' },
		{ EKeys::SpaceBar, ' ' },
		{ EKeys::Enter, KEY_ENTER }, { EKeys::Escape, KEY_ESCAPE }, { EKeys::Tab, KEY_TAB },
		{ EKeys::BackSpace, KEY_BACKSPACE },
		{ EKeys::Left, KEY_LEFT }, { EKeys::Up, KEY_UP }, { EKeys::Right, KEY_RIGHT }, { EKeys::Down, KEY_DOWN },
		{ EKeys::F1, KEY_F1 }, { EKeys::F2, KEY_F2 }, { EKeys::F3, KEY_F3 }, { EKeys::F4, KEY_F4 },
		{ EKeys::F5, KEY_F5 }, { EKeys::F6, KEY_F6 }, { EKeys::F7, KEY_F7 }, { EKeys::F8, KEY_F8 },
		{ EKeys::F9, KEY_F9 }, { EKeys::F10, KEY_F10 }, { EKeys::F11, KEY_F11 }, { EKeys::F12, KEY_F12 },
	};

	if (const int32* found = table.Find(key)) return *found;
	return 0;
}

int32 UPuzzleWidget::TranslateMouseButton(const FKey& key) {
	if (key == EKeys::LeftMouseButton) return MOUSE_LEFT;
	if (key == EKeys::RightMouseButton) return MOUSE_RIGHT;
	if (key == EKeys::MiddleMouseButton) return MOUSE_MIDDLE;
	return -1;
}

FIntPoint UPuzzleWidget::GetViewportSize(int32 fallbackWidth, int32 fallbackHeight) {
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport) {
		FIntPoint viewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
		if (viewportSize.X > 0 && viewportSize.Y > 0) return viewportSize;
	}
	return FIntPoint(fallbackWidth, fallbackHeight);
}
