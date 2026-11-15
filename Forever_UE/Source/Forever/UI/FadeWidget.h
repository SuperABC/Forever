#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FadeWidget.generated.h"

class AForeverFrameworkActor;
class UProgressBar;
class UTextBlock;

// 开局黑屏：全黑遮罩+加载进度条+状态文字，世界就绪后渐隐并通知UFadeSubsystem移除，详见FadeWidget.md。
UCLASS()
class FOREVER_API UFadeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 回到全黑+等待就绪状态(每次重新变黑时调用)。
	void ResetToBlack();

	// 游戏内黑屏(跳时间)：从透明渐变到全黑，之后保持黑屏并显示框架的跳时间进度(AForeverFrameworkActor::
	// GetTimeSkipProgress)，直到Release。Release后进度条补满再渐隐，渐隐完成触发onFinished。
	void BeginHold();
	void Release();
	bool IsHeldBlack() const { return state == EState::Holding; }

	// 渐隐完成时触发(控件此时已经完全透明)，由UFadeSubsystem用来把它从视口移除。
	FSimpleDelegate onFinished;

protected:
	virtual void NativeConstruct() override;
	// InDeltaTime是真实时间——等待期间世界被SetGlobalTimeDilation(0)冻结，渐隐不能用世界时间。
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// BeginHold时从透明渐变到全黑用的秒数。
	UPROPERTY(EditDefaultsOnly, Category = "Fade")
	float fadeToBlackDuration = 0.6f;

	// 世界就绪后，从全黑渐变到完全透明用的秒数。
	UPROPERTY(EditDefaultsOnly, Category = "Fade")
	float fadeDuration = 1.5f;

	// 进度条显示值追赶目标值的速度(百分比/秒，1=满条)：每完成一格让条滑过去，而不是瞬跳。
	UPROPERTY(EditDefaultsOnly, Category = "Progress")
	float barInterpSpeed = 3.f;

	// 进度条(0..1)。WBP里放一个叫ProgressBar的Progress Bar(勾Is Variable)；没放则不显示进度条。
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar;

	// "构建地形 (2/18)"这类状态文字。WBP里放一个叫StatusText的TextBlock(勾Is Variable)；没放则不显示。
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	enum class EState : uint8 { Waiting, Fading, Done, FadingToBlack, Holding };

	// 取当前World里的FrameworkActor；还没出现(关卡还在加载)返回nullptr。
	AForeverFrameworkActor* FindFramework(UWorld* world);

	// 把进度条/文字刷新到当前进度；返回世界是否已经完全就绪(进度满格)。
	bool UpdateProgress(float deltaTime, AForeverFrameworkActor* frameworkActor);

	EState state = EState::Waiting;
	float elapsed = 0.f;
	float displayedPercent = 0.f;
	bool bReleaseRequested = false;
	TWeakObjectPtr<UWorld> trackedWorld;
	TWeakObjectPtr<AForeverFrameworkActor> framework;
};
