#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForeverFadeWidget.generated.h"

class AForeverFrameworkActor;

// 开局黑屏：盖住整个画面的全黑遮罩，世界就绪后整体渐变透明，渐隐完通知UForeverFadeSubsystem移除。
// 控件由UForeverFadeSubsystem持有并挂在GameViewport上(跨关卡存活)，按下开始/加载按钮时立刻变黑，
// 一直黑到World关卡的世界就绪，见ForeverFadeSubsystem.h。
// 就绪 = AForeverFrameworkActor的世界生成流程走完(新游戏即所有game_start事件响应完，读档即存档里
// 的玩家/世界物品恢复完) 且 建筑LOD已稳定(IsLodSettled，此时场景和人物都已加载完)。
// 新游戏和读档共用同一个条件。
// 逻辑都在C++里，外观由Blueprint子类(WBP_Fade)设计：放一个全屏纯黑的Border/Image即可，淡出作用在
// 控件根的RenderOpacity上，不需要BindWidget。以后加加载进度条也在WBP里摆。详见ForeverFadeWidget.md。
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

	// 兜底：进入World关卡后等待超过这么多秒(真实时间)还没就绪，也开始渐隐，避免世界生成异常/没有
	// 玩家可操控导致LOD冻结从未被请求时永远黑屏。关卡切换的加载时间不计入(换了World就重新计时)。
	UPROPERTY(EditDefaultsOnly, Category = "Fade")
	float maxWaitSeconds = 15.f;

private:
	enum class EState : uint8 { Waiting, Fading, Done };

	// 世界是否就绪；当前World里FrameworkActor还没出现(关卡还没加载完)时算没就绪。
	bool IsWorldReady(UWorld* world);

	EState state = EState::Waiting;
	float elapsed = 0.f;
	TWeakObjectPtr<UWorld> trackedWorld;
	TWeakObjectPtr<AForeverFrameworkActor> framework;
};
