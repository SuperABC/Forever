#include "ForeverFadeSubsystem.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"

#include "UI/ForeverFadeWidget.h"

namespace {
	// 高于PausePanel(10)，低于背包拖拽ghost(1000)。
	constexpr int32 kFadeZOrder = 100;
	// OpenLevel之前等的帧数：变黑后至少要真正渲染出一帧，OpenLevel才会开始阻塞。
	constexpr int32 kFramesBeforeOpenLevel = 3;
}

void UForeverFadeSubsystem::Deinitialize() {
	RemoveFromViewport();
	Super::Deinitialize();
}

void UForeverFadeSubsystem::RegisterWidgetClass(TSubclassOf<UForeverFadeWidget> cls) {
	if (cls != nullptr && cls != widgetClass) {
		widgetClass = cls;
		// 类换了(两个Controller指定的不是同一个WBP)，旧控件作废。
		RemoveFromViewport();
		widget = nullptr;
	}
}

void UForeverFadeSubsystem::ShowBlack() {
	if (widgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverFadeSubsystem: 没有任何Controller登记fadeWidgetClass，不显示开局黑屏。"));
		return;
	}
	UGameInstance* gameInstance = GetGameInstance();
	UGameViewportClient* viewport = gameInstance ? gameInstance->GetGameViewportClient() : nullptr;
	if (viewport == nullptr) return;

	if (widget == nullptr) {
		widget = CreateWidget<UForeverFadeWidget>(gameInstance, widgetClass);
		if (widget == nullptr) return;
		widget->onFinished.BindUObject(this, &UForeverFadeSubsystem::HandleFinished);
	}
	widget->ResetToBlack();
	if (!bShown) {
		viewport->AddViewportWidgetContent(widget->TakeWidget(), kFadeZOrder);
		bShown = true;
	}
}

void UForeverFadeSubsystem::EnsureBlack() {
	if (!bShown) ShowBlack();
}

void UForeverFadeSubsystem::OpenWorldLevel() {
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

void UForeverFadeSubsystem::HandleFinished() {
	RemoveFromViewport();
}

void UForeverFadeSubsystem::RemoveFromViewport() {
	if (!bShown) return;
	bShown = false;
	UGameInstance* gameInstance = GetGameInstance();
	UGameViewportClient* viewport = gameInstance ? gameInstance->GetGameViewportClient() : nullptr;
	if (viewport && widget) {
		viewport->RemoveViewportWidgetContent(widget->TakeWidget());
	}
}
