#include "ForeverPlayerController.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

AForeverPlayerController::AForeverPlayerController()
{
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> defaultContextFinder(
		TEXT("/Game/Blueprint/Player/Input/IMC_Default.IMC_Default"));
	if (defaultContextFinder.Succeeded()) {
		defaultMappingContexts.Add(defaultContextFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> mouseLookContextFinder(
		TEXT("/Game/Blueprint/Player/Input/IMC_MouseLook.IMC_MouseLook"));
	if (mouseLookContextFinder.Succeeded()) {
		defaultMappingContexts.Add(mouseLookContextFinder.Object);
	}
}

void AForeverPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (IsLocalPlayerController()) {
		if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer())) {
			for (UInputMappingContext* context : defaultMappingContexts) {
				subsystem->AddMappingContext(context, 0);
			}
		}
	}
}
