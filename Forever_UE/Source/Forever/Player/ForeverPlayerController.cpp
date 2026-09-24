#include "ForeverPlayerController.h"

#include "UI/MeetOptionWidget.h"
#include "UI/SectionSpeakingWidget.h"
#include "UI/SectionOptionWidget.h"
#include "UI/PuzzleWidget.h"

void AForeverPlayerController::BeginPlay() {
	Super::BeginPlay();

	if (meetOptionWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: meetOptionWidgetClass未设置,跳过MeetOption创建。"));
	}
	else if ((meetOptionWidget = CreateWidget<UMeetOptionWidget>(this, meetOptionWidgetClass)) != nullptr) {
		meetOptionWidget->AddToViewport();
	}

	if (sectionSpeakingWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: sectionSpeakingWidgetClass未设置,跳过SectionSpeaking创建。"));
	}
	else if ((sectionSpeakingWidget = CreateWidget<USectionSpeakingWidget>(this, sectionSpeakingWidgetClass)) != nullptr) {
		sectionSpeakingWidget->AddToViewport();
		sectionSpeakingWidget->HideLine();
	}

	if (sectionOptionWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: sectionOptionWidgetClass未设置,跳过SectionOption创建。"));
	}
	else if ((sectionOptionWidget = CreateWidget<USectionOptionWidget>(this, sectionOptionWidgetClass)) != nullptr) {
		sectionOptionWidget->AddToViewport();
		sectionOptionWidget->HideOptions();
	}

	if (puzzleWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: puzzleWidgetClass未设置,跳过PuzzleWidget创建。"));
	}
	else if ((puzzleWidget = CreateWidget<UPuzzleWidget>(this, puzzleWidgetClass)) != nullptr) {
		puzzleWidget->AddToViewport();
	}
}
