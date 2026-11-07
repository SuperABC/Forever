// Copyright Epic Games, Inc. All Rights Reserved.

#include "Forever.h"
#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "AI/NavigationSystemConfig.h"

// 在每个World初始化之前关掉UE自带的导航系统(项目用Core导航图)，原因和时序见Forever.md。
class FForeverGameModule : public FDefaultGameModuleImpl {
public:
	virtual void StartupModule() override {
		preWorldInitHandle = FWorldDelegates::OnPreWorldInitialization.AddStatic(&FForeverGameModule::DisableNavigation);
	}

	virtual void ShutdownModule() override {
		FWorldDelegates::OnPreWorldInitialization.Remove(preWorldInitHandle);
	}

private:
	static void DisableNavigation(UWorld* world, const UWorld::InitializationValues) {
		AWorldSettings* worldSettings = world ? world->GetWorldSettings() : nullptr;
		if (!worldSettings || worldSettings->GetNavigationSystemConfigOverride()) return;
		worldSettings->SetNavigationSystemConfigOverride(NewObject<UNullNavSysConfig>(worldSettings, NAME_None, RF_Transient));
	}

	FDelegateHandle preWorldInitHandle;
};

IMPLEMENT_PRIMARY_GAME_MODULE( FForeverGameModule, Forever, "Forever" );
