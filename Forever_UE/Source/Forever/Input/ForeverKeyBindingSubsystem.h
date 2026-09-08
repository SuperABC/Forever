#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ForeverKeyBindingSubsystem.generated.h"

class FConfigFile;
class UInputAction;
class UInputMappingContext;

UCLASS()
class FOREVER_API UForeverKeyBindingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UInputAction* GetAction(FName bindingName) const;
	UInputMappingContext* GetBindingContext() const { return bindingContext; }

private:
	struct FBindingDefinition
	{
		FName name;
		FKey defaultKey;
	};

	static const TArray<FBindingDefinition>& GetBindingDefinitions();

	FKey ResolveKey(const FConfigFile& configFile, const FBindingDefinition& definition) const;
	void BuildDynamicContext(const FConfigFile& configFile);

	UPROPERTY()
	TObjectPtr<UInputMappingContext> bindingContext;

	UPROPERTY()
	TMap<FName, TObjectPtr<UInputAction>> actions;
};
