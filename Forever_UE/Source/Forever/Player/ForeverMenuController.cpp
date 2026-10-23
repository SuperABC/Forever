#include "ForeverMenuController.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/GameInstance.h"
#include "Framework/ForeverMemoryReport.h"
#include "UI/ForeverFadeSubsystem.h"
#include "UI/ForeverFadeWidget.h"
#include "UObject/UObjectGlobals.h"

void AForeverMenuController::BeginPlay()
{
	Super::BeginPlay();

	// 每次进入菜单(开局进来或从游戏里退出游戏回来)先强制GC，再打一行内存快照，反复进出几轮
	// 对比这一行就能看出有没有泄漏，见ForeverMemoryReport.md。
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	ForeverMemoryReport::Log(GetWorld(), TEXT("menu"));

	if (UGameInstance* gameInstance = GetGameInstance()) {
		if (UForeverFadeSubsystem* fade = gameInstance->GetSubsystem<UForeverFadeSubsystem>()) {
			if (fadeWidgetClass == nullptr) {
				UE_LOG(LogTemp, Warning, TEXT("AForeverMenuController: fadeWidgetClass未设置,按下开始/加载按钮时不会立刻变黑。"));
			}
			fade->RegisterWidgetClass(fadeWidgetClass);
		}
	}

	if (startMenuWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverMenuController: startMenuWidgetClass未设置,跳过主菜单创建。"));
		return;
	}

	if (UUserWidget* startMenu = CreateWidget<UUserWidget>(this, startMenuWidgetClass)) {
		startMenu->AddToViewport();

		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, startMenu, EMouseLockMode::DoNotLock, false);
		bShowMouseCursor = true;
	}
}
