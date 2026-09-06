#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ForeverPlayerController.generated.h"

class UInputMappingContext;

UCLASS()
class FOREVER_API AForeverPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AForeverPlayerController();

protected:
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<TObjectPtr<UInputMappingContext>> defaultMappingContexts;
};
