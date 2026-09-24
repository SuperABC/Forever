#include "ForeverPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

#include "Input/ForeverKeyBindingSubsystem.h"
#include "UI/MeetOptionWidget.h"
#include "UI/SectionSpeakingWidget.h"
#include "UI/SectionOptionWidget.h"
#include "UI/PuzzleWidget.h"
#include "UI/PhoneWidget.h"

void AForeverPlayerController::BeginPlay() {
	Super::BeginPlay();

	// 手机(Phone)系统：P键要求任何时候都生效(包括开局还没占有任何真正Pawn、操控着
	// ADefaultPawn的那几帧，以及ChangeControlChange换人操控/换乘载具的瞬间)——这个动态
	// Context原来只在Pawn的PossessedBy里加(见AForeverCharacter::PossessedBy)，Controller
	// 自己在这里常驻加一份，就不用依赖"当前是不是正占有着一个真正的Pawn"，见
	// MAINCONTROLLER_TODO.md"P开手机"这一行热键的既定设计。Pawn那边同样这两行现在删掉了
	// (不需要重复加)，见AForeverCharacter.cpp/VehicleElement.cpp。
	if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer())) {
		if (UGameInstance* gameInstance = GetGameInstance()) {
			if (UForeverKeyBindingSubsystem* keyBindings = gameInstance->GetSubsystem<UForeverKeyBindingSubsystem>()) {
				subsystem->AddMappingContext(keyBindings->GetBindingContext(), 0);
			}
		}
	}

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

	if (phoneWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: phoneWidgetClass未设置,跳过PhoneWidget创建。"));
	}
	else if ((phoneWidget = CreateWidget<UPhoneWidget>(this, phoneWidgetClass)) != nullptr) {
		phoneWidget->AddToViewport();
	}
}

void AForeverPlayerController::SetupInputComponent() {
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* enhancedInput = Cast<UEnhancedInputComponent>(InputComponent)) {
		if (UGameInstance* gameInstance = GetGameInstance()) {
			if (UForeverKeyBindingSubsystem* keyBindings = gameInstance->GetSubsystem<UForeverKeyBindingSubsystem>()) {
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("TogglePhone")), ETriggerEvent::Started, this, &AForeverPlayerController::TogglePhone);
			}
		}
	}
}

void AForeverPlayerController::TogglePhone() {
	if (phoneWidget) phoneWidget->TogglePhone();
}
