#include "ForeverFadeWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

#include "Framework/ForeverFrameworkActor.h"

namespace {
	// 找不到FrameworkActor(OpenLevel还在阻塞加载/关卡刚进来)时显示的文字。
	const TCHAR* kLoadingText = TEXT("正在载入…");
}

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
	settleElapsed = 0.f;
	displayedPercent = 0.f;
	trackedWorld.Reset();
	framework.Reset();
	if (ProgressBar) ProgressBar->SetPercent(0.f);
	if (StatusText) StatusText->SetText(FText::FromString(kLoadingText));
}

AForeverFrameworkActor* UForeverFadeWidget::FindFramework(UWorld* world) {
	if (world == nullptr) return nullptr;
	if (!framework.IsValid()) {
		for (TActorIterator<AForeverFrameworkActor> it(world); it; ++it) {
			framework = *it;
			break;
		}
	}
	return framework.Get();
}

bool UForeverFadeWidget::UpdateProgress(float deltaTime, AForeverFrameworkActor* frameworkActor) {
	int32 done = 0, total = 0;
	FText label;
	if (frameworkActor) frameworkActor->GetLoadProgress(done, total, label);

	const float target = total > 0 ? static_cast<float>(done) / total : 0.f;
	displayedPercent = FMath::FInterpConstantTo(displayedPercent, target, deltaTime, barInterpSpeed);
	if (ProgressBar) ProgressBar->SetPercent(displayedPercent);

	if (StatusText) {
		if (total > 0) {
			// 序号是"正在做第几步"：已完成done格时正在做第done+1格，全部完成显示total/total。
			StatusText->SetText(FText::FromString(FString::Printf(TEXT("%s  (%d/%d)"),
				*label.ToString(), FMath::Min(done + 1, total), total)));
		}
		else {
			StatusText->SetText(FText::FromString(kLoadingText));
		}
	}

	// 进度满格(含显示值也已经滑到头)才算就绪，让最后一格也能被看见再渐隐。
	return total > 0 && done >= total && displayedPercent >= 0.999f;
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
			settleElapsed = 0.f;
		}
		AForeverFrameworkActor* frameworkActor = FindFramework(world);
		elapsed += InDeltaTime;

		bool bTimeout = false;
		if (frameworkActor && frameworkActor->IsInitialGenerationDone()) {
			settleElapsed += InDeltaTime;
			bTimeout = settleElapsed >= maxWaitSeconds;
		}
		else if (!frameworkActor) {
			bTimeout = elapsed >= maxWorldWaitSeconds;
		}

		if (UpdateProgress(InDeltaTime, frameworkActor)) {
			state = EState::Fading;
			elapsed = 0.f;
		}
		else if (bTimeout) {
			UE_LOG(LogTemp, Warning, TEXT("UForeverFadeWidget: 等待世界就绪超时，强制开始渐隐。"));
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
