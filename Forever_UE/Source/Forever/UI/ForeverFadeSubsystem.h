#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ForeverFadeSubsystem.generated.h"

class UForeverFadeWidget;

// 开局黑屏的持有者。黑屏必须在"按下开始/加载按钮"的那一帧就出现，而OpenLevel(World)要同步加载
// 很久、期间不渲染，所以：
// 1. 控件由GameInstance持有(不属于任何关卡)，直接挂在GameViewportClient上——不随关卡销毁，
//    Menu→World切换过程中一直盖着，World的世界就绪后自己渐隐(UForeverFadeWidget)；
// 2. OpenWorldLevel先变黑，等几帧让黑屏真正画出来，再OpenLevel。
// WBP_Fade的类由Menu/World两个Controller在BeginPlay里RegisterWidgetClass登记(类在各自的BP里指定)。
UCLASS()
class FOREVER_API UForeverFadeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// Controller在BeginPlay里登记WBP_Fade的类；cls为空(没配置)时忽略。
	void RegisterWidgetClass(TSubclassOf<UForeverFadeWidget> cls);

	// 立刻变黑(本帧)：已经显示就重置回全黑。没登记过类则打警告、什么也不做。
	void ShowBlack();

	// World关卡进场时调用：黑屏还没显示(比如编辑器里直接PIE在World.umap上)就补一个。
	void EnsureBlack();

	// 变黑，等几帧让黑屏画出来，再OpenLevel(World)。开始游戏/读档共用。
	void OpenWorldLevel();

private:
	void HandleFinished();
	void RemoveFromViewport();

	UPROPERTY()
	TSubclassOf<UForeverFadeWidget> widgetClass;

	UPROPERTY()
	TObjectPtr<UForeverFadeWidget> widget;

	// 控件当前挂在视口上(黑着或渐隐中)。
	bool bShown = false;
};
