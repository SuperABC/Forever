#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForeverFadeWidget.generated.h"

class AForeverFrameworkActor;
class UProgressBar;
class UTextBlock;

// 开局黑屏：盖住整个画面的全黑遮罩，上面显示加载进度条和"正在构建什么"的文字；世界就绪后整体渐变透明，
// 渐隐完通知UForeverFadeSubsystem移除。
// 控件由UForeverFadeSubsystem持有并挂在GameViewport上(跨关卡存活)，按下开始/加载按钮时立刻变黑，
// 一直黑到World关卡的世界就绪，见ForeverFadeSubsystem.h。
// 进度 = AForeverFrameworkActor::GetLoadProgress：世界生成拆成若干阶段(构建地形/路网/园区/建筑/人口…)，
// 每完成一个阶段进度条前进一格，最后一格是"构建场景"(进图后等建筑LOD排空)。进度走满后才开始渐隐。
// 逻辑都在C++里，外观由Blueprint子类(WBP_Fade)设计：一个全屏纯黑的Border/Image做底，再摆上面
// 两个可选控件(BindWidgetOptional)；淡出作用在控件根的RenderOpacity上。详见ForeverFadeWidget.md。
UCLASS()
class FOREVER_API UForeverFadeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 回到全黑+等待就绪状态(每次重新变黑时调用)。
	void ResetToBlack();

	// 渐隐完成时触发(控件此时已经完全透明)，由UForeverFadeSubsystem用来把它从视口移除。
	FSimpleDelegate onFinished;

protected:
	virtual void NativeConstruct() override;
	// InDeltaTime是真实时间——等待期间世界被SetGlobalTimeDilation(0)冻结，渐隐不能用世界时间。
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 世界就绪后，从全黑渐变到完全透明用的秒数。
	UPROPERTY(EditDefaultsOnly, Category = "Fade")
	float fadeDuration = 1.5f;

	// 兜底：世界生成流程走完之后(等"构建场景"那一格)超过这么多秒(真实时间)还没排空，也开始渐隐，
	// 避免没有玩家可操控导致LOD冻结从未被请求时永远黑屏。生成阶段本身不计时——它自己有异常退出。
	UPROPERTY(EditDefaultsOnly, Category = "Fade")
	float maxWaitSeconds = 15.f;

	// 兜底：进入World之后一直找不到AForeverFrameworkActor(关卡不对/没生成)超过这么多秒也开始渐隐。
	UPROPERTY(EditDefaultsOnly, Category = "Fade")
	float maxWorldWaitSeconds = 120.f;

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
	enum class EState : uint8 { Waiting, Fading, Done };

	// 取当前World里的FrameworkActor；还没出现(关卡还在加载)返回nullptr。
	AForeverFrameworkActor* FindFramework(UWorld* world);

	// 把进度条/文字刷新到当前进度；返回世界是否已经完全就绪(进度满格)。
	bool UpdateProgress(float deltaTime, AForeverFrameworkActor* frameworkActor);

	EState state = EState::Waiting;
	float elapsed = 0.f;
	// 已经开始等"构建场景"(生成流程走完)之后的秒数，兜底用。
	float settleElapsed = 0.f;
	float displayedPercent = 0.f;
	TWeakObjectPtr<UWorld> trackedWorld;
	TWeakObjectPtr<AForeverFrameworkActor> framework;
};
