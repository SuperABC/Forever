#include "ForeverPlayerController.h"

#include "UI/MeetOptionWidget.h"

void AForeverPlayerController::BeginPlay() {
	Super::BeginPlay();

	if (meetOptionWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: meetOptionWidgetClass未设置,跳过MeetOption创建。"));
		return;
	}

	meetOptionWidget = CreateWidget<UMeetOptionWidget>(this, meetOptionWidgetClass);
	if (meetOptionWidget) {
		meetOptionWidget->AddToViewport();
	}
}
