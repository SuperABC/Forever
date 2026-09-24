#include "UI/PhoneWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "UI/CanvasInputTranslation.h"
#include "Framework/ForeverFrameworkActor.h"
#include "common/implement.h"

#undef UpdateResource


void UPhoneWidget::NativeConstruct() {
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);

	// 跟PuzzleWidget同样的理由(见PuzzleWidget.cpp里NativeConstruct的说明)：树里只有一张
	// 纯展示用的Image，没有任何天生可获得焦点的子控件，不打开这个开关键盘会完全没反应。
	SetIsFocusable(true);
}

void UPhoneWidget::NativeDestruct() {
	delete phone;
	phone = nullptr;

	Super::NativeDestruct();
}

void UPhoneWidget::TogglePhone() {
	if (GetVisibility() == ESlateVisibility::Visible) ClosePhone();
	else OpenPhone();
}

FIntPoint UPhoneWidget::ComputeCanvasSize() const {
	if (DisplayImage) {
		FVector2D absoluteSize = DisplayImage->GetCachedGeometry().GetAbsoluteSize();
		if (absoluteSize.X > 0.f && absoluteSize.Y > 0.f) {
			return FIntPoint(FMath::RoundToInt(absoluteSize.X), FMath::RoundToInt(absoluteSize.Y));
		}
	}
	return FIntPoint(kFallbackPhoneWidth, kFallbackPhoneHeight);
}

void UPhoneWidget::OpenPhone() {
	if (!phone) {
		FIntPoint size = ComputeCanvasSize();
		canvas.Init(size.X, size.Y);
		phone = new Phone();
		phone->Init(size.X, size.Y);
	}

	SetVisibility(ESlateVisibility::Visible);

	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = true;
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(playerController, this, EMouseLockMode::DoNotLock, false);
		SetKeyboardFocus();
	}
}

void UPhoneWidget::ClosePhone() {
	SetVisibility(ESlateVisibility::Collapsed);

	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = false;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(playerController);
		UWidgetBlueprintLibrary::SetFocusToGameViewport();
	}

	// 不delete phone/canvas——手机要跨越开关手机保持多任务状态(桌面/正在跑的App/最近任务
	// 列表)，只有Widget自己被销毁(NativeDestruct)时才真正释放，见类注释第1点。
}

void UPhoneWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime) {
	Super::NativeTick(MyGeometry, DeltaTime);

	if (!phone || GetVisibility() != ESlateVisibility::Visible) return;

	// 每帧比对一次DisplayImage的真实屏幕像素尺寸——跟PuzzleWidget一样的resize检测手法，
	// 只是这次比对的是子控件的footprint而不是整个视口。canvas.Resize内部只在尺寸真的变了时
	// 才重新分配缓冲区；phone->Resize同步重算布局(网格/工具栏等按比例的位置)，不重置状态。
	FIntPoint size = ComputeCanvasSize();
	if (size.X != canvas.GetWidth() || size.Y != canvas.GetHeight()) {
		canvas.Resize(size.X, size.Y);
		phone->Resize(size.X, size.Y);
	}

	// 现场构造一个PostImplement给Phone主屏查"game time"用——很轻量(7个裸指针+一个JsonValue
	// 缓存)，不用持久持有。找不到AForeverFrameworkActor(理论上不会发生)时对应域指针传
	// nullptr，PostImplement::Post自己会处理成查询失败，Phone::RenderHome那边已经做了
	// result["result"]=="success"的判断。
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	PostImplement post(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		framework ? framework->GetStory() : nullptr,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);

	int result = phone->Loop(&canvas, FMath::RoundToInt(DeltaTime * 1000.f), &post);
	BlitCanvasToImage();

	if (result != 0) ClosePhone();
}

void UPhoneWidget::BlitCanvasToImage() {
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

FReply UPhoneWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) {
	int32 code = CanvasInputTranslation::TranslateKey(InKeyEvent.GetKey());
	if (code != 0) canvas.PushKey(code);
	return FReply::Handled();
}

FReply UPhoneWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) {
	int32 code = CanvasInputTranslation::TranslateKey(InKeyEvent.GetKey());
	if (code != 0) canvas.PushKey(code | KEY_RELEASED_FLAG);
	return FReply::Handled();
}

FReply UPhoneWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	// 不能用InGeometry(整个UPhoneWidget自己的几何信息)做AbsoluteToLocal——跟PuzzleWidget
	// 不一样，Phone的DisplayImage这次特意做成手机形状、不铺满整个Widget(见类注释)，如果
	// DisplayImage在Widget里有偏移，按Widget自己的原点算出来的本地坐标会跟画布像素坐标对不
	// 上，表现为"点哪里都像是点在了画布的另一个位置"。改成用DisplayImage自己的CachedGeometry
	// 做转换。
	//
	// 但只做AbsoluteToLocal还不够：它算出来的是这个Geometry的"本地Slate单位"坐标，跟
	// GetAbsoluteSize()(真实屏幕像素，ComputeCanvasSize()用来定画布分辨率的那个)不是同一个
	// 度量——两者之间差一个这个控件自己的累积缩放比例(GetLocalSize()跟GetAbsoluteSize()的
	// 比值，通常来自UMG的DPI Scale曲线)，且这个比例对x/y是同一个值，正好对应"点击位置跟
	// 实际位置差一个固定比例(不是固定偏移)"这个症状——之前把AbsoluteToLocal的结果直接当画布
	// 像素坐标用，相当于漏乘了这个缩放比例。这里显式按LocalSize/AbsoluteSize的比例把本地坐标
	// 换算成画布像素坐标，跟ComputeCanvasSize()用的是同一个基准。
	if (!DisplayImage) return FReply::Handled();
	FGeometry displayGeometry = DisplayImage->GetCachedGeometry();
	FVector2D localPos = displayGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	FVector2D localSize = displayGeometry.GetLocalSize();
	FVector2D absoluteSize = displayGeometry.GetAbsoluteSize();

	float canvasX = localPos.X, canvasY = localPos.Y;
	if (localSize.X > 0.f && localSize.Y > 0.f) {
		canvasX = localPos.X * (absoluteSize.X / localSize.X);
		canvasY = localPos.Y * (absoluteSize.Y / localSize.Y);
	}
	canvas.SetMousePos(FMath::RoundToInt(canvasX), FMath::RoundToInt(canvasY));
	return FReply::Handled();
}

FReply UPhoneWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	int32 button = CanvasInputTranslation::TranslateMouseButton(InMouseEvent.GetEffectingButton());
	if (button >= 0) canvas.PushMouseButton(button, true);
	return FReply::Handled();
}

FReply UPhoneWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	int32 button = CanvasInputTranslation::TranslateMouseButton(InMouseEvent.GetEffectingButton());
	if (button >= 0) canvas.PushMouseButton(button, false);
	return FReply::Handled();
}
