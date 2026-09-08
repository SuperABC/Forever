#include "ForeverMenuController.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

void AForeverMenuController::BeginPlay()
{
	Super::BeginPlay();

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
