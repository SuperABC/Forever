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
	// 某个绑定实际生效的按键(读配置后的结果)——UI Only模式下的widget(比如暂停界面)收不到
	// Enhanced Input，要在NativeOnKeyDown里自己比对按键时用。未知名字返回EKeys::Invalid。
	FKey GetKey(FName bindingName) const;
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

	TMap<FName, FKey> keys;
};
