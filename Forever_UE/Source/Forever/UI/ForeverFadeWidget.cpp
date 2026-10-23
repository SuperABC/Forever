#include "ForeverFadeWidget.h"

#include "Engine/GameInstance.h"
#include "EngineUtils.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverBuildingFrameworkComponent.h"

void UForeverFadeWidget::NativeConstruct() {
	Super::NativeConstruct();
	ResetToBlack();
}

void UForeverFadeWidget::ResetToBlack() {
	// 纯视觉遮罩，不拦截鼠标/键盘。
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(1.f);
	state = EState::Waiting;
	elapsed = 0.f;
	trackedWorld.Reset();
	framework.Reset();
}

bool UForeverFadeWidget::IsWorldReady(UWorld* world) {
	if (world == nullptr) return false;
	if (!framework.IsValid()) {
		for (TActorIterator<AForeverFrameworkActor> it(world); it; ++it) {
			framework = *it;
			break;
		}
		if (!framework.IsValid()) return false;
	}
	if (!framework->IsInitialGenerationDone()) return false;
	UForeverBuildingFrameworkComponent* building = framework->GetBuildingFramework();
	return building == nullptr || building->IsLodSettled();
}

void UForeverFadeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime) {
	Super::NativeTick(MyGeometry, InDeltaTime);

	switch (state) {
	case EState::Waiting: {
		// 控件跨关卡存活，World换了就重新找FrameworkActor、重新计兜底时间。
		UGameInstance* gameInstance = GetGameInstance();
		UWorld* world = gameInstance ? gameInstance->GetWorld() : nullptr;
		if (world != trackedWorld.Get()) {
			trackedWorld = world;
			framework.Reset();
			elapsed = 0.f;
		}
		elapsed += InDeltaTime;
		if (IsWorldReady(world)) {
			state = EState::Fading;
			elapsed = 0.f;
		}
		else if (elapsed >= maxWaitSeconds) {
			UE_LOG(LogTemp, Warning, TEXT("UForeverFadeWidget: 等待世界就绪超过%.1f秒，强制开始渐隐。"), maxWaitSeconds);
			state = EState::Fading;
			elapsed = 0.f;
		}
		break;
	}
	case EState::Fading: {
		elapsed += InDeltaTime;
		const float alpha = fadeDuration > KINDA_SMALL_NUMBER ? 1.f - elapsed / fadeDuration : 0.f;
		if (alpha <= 0.f) {
			state = EState::Done;
			SetRenderOpacity(0.f);
			onFinished.ExecuteIfBound();
		}
		else {
			SetRenderOpacity(alpha);
		}
		break;
	}
	case EState::Done:
		break;
	}
}
