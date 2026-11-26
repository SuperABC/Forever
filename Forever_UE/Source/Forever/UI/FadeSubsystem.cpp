#include "FadeSubsystem.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"

#include "UI/FadeWidget.h"

namespace {
	// 高于PausePanel(10)，低于背包拖拽ghost(1000)。
	constexpr int32 kFadeZOrder = 100;
	// OpenLevel之前等的帧数：变黑后至少要真正渲染出一帧，OpenLevel才会开始阻塞。
	constexpr int32 kFramesBeforeOpenLevel = 3;
}

void UFadeSubsystem::Deinitialize() {
	RemoveFromViewport();
	Super::Deinitialize();
}

void UFadeSubsystem::RegisterWidgetClass(TSubclassOf<UFadeWidget> cls) {
	if (cls != nullptr && cls != widgetClass) {
		widgetClass = cls;
		// 类换了(两个Controller指定的不是同一个WBP)，旧控件作废。
		RemoveFromViewport();
		widget = nullptr;
	}
}

UFadeWidget* UFadeSubsystem::EnsureWidgetShown() {
	if (widgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("UFadeSubsystem: 没有任何Controller登记fadeWidgetClass，不显示黑屏。"));
		return nullptr;
	}
	UGameInstance* gameInstance = GetGameInstance();
	UGameViewportClient* viewport = gameInstance ? gameInstance->GetGameViewportClient() : nullptr;
	if (viewport == nullptr) return nullptr;

	if (widget == nullptr) {
		widget = CreateWidget<UFadeWidget>(gameInstance, widgetClass);
		if (widget == nullptr) return nullptr;
		widget->onFinished.BindUObject(this, &UFadeSubsystem::HandleFinished);
	}
	return widget;
}

void UFadeSubsystem::ShowBlack() {
	UFadeWidget* fade = EnsureWidgetShown();
	if (fade == nullptr) return;
	fade->ResetToBlack();
	if (!bShown) {
		GetGameInstance()->GetGameViewportClient()->AddViewportWidgetContent(fade->TakeWidget(), kFadeZOrder);
		bShown = true;
	}
}

bool UFadeSubsystem::BeginHoldBlack() {
	UFadeWidget* fade = EnsureWidgetShown();
	if (fade == nullptr) return false;
	// 先TakeWidget再BeginHold：控件之前被RemoveFromViewport释放过Slate的话，TakeWidget会重建并触发NativeConstruct，
	// 而NativeConstruct会ResetToBlack把状态打回Waiting——先BeginHold再TakeWidget的话，保持黑屏的状态就被冲掉了，
	// 随后控件按"加载完成"走Fading→Done自己消失，跳时间永远等不到IsHeldBlack。
	TSharedRef<SWidget> slateWidget = fade->TakeWidget();
	fade->BeginHold();
	if (!bShown) {
		GetGameInstance()->GetGameViewportClient()->AddViewportWidgetContent(slateWidget, kFadeZOrder);
		bShown = true;
	}
	return true;
}

bool UFadeSubsystem::IsHeldBlack() const {
	return bShown && widget != nullptr && widget->IsHeldBlack();
}

void UFadeSubsystem::ReleaseHold() {
	if (bShown && widget != nullptr) widget->Release();
}

void UFadeSubsystem::EnsureBlack() {
	if (!bShown) ShowBlack();
}

void UFadeSubsystem::OpenWorldLevel() {
	ShowBlack();

	UGameInstance* gameInstance = GetGameInstance();
	if (gameInstance == nullptr) return;
	TWeakObjectPtr<UGameInstance> weakInstance = gameInstance;
	int32 framesLeft = kFramesBeforeOpenLevel;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([weakInstance, framesLeft](float) mutable {
		if (--framesLeft > 0) return true;
		if (UGameInstance* instance = weakInstance.Get()) {
			UGameplayStatics::OpenLevel(instance, FName(TEXT("/Game/Levels/World.World")));
		}
		return false;
	}));
}

void UFadeSubsystem::Dismiss() {
	RemoveFromViewport();
}

void UFadeSubsystem::HandleFinished() {
	RemoveFromViewport();
}

void UFadeSubsystem::RemoveFromViewport() {
	if (!bShown) return;
	bShown = false;
	UGameInstance* gameInstance = GetGameInstance();
	UGameViewportClient* viewport = gameInstance ? gameInstance->GetGameViewportClient() : nullptr;
	if (viewport && widget) {
		viewport->RemoveViewportWidgetContent(widget->TakeWidget());
	}
}
