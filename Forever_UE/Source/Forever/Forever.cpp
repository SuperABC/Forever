// Copyright Epic Games, Inc. All Rights Reserved.

#include "Forever.h"
#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "AI/NavigationSystemConfig.h"

// 这个项目所有导航(车行/行人寻路)都由Core的导航图自己实现，不用UE自带的NavigationSystem/
// NavMesh。DefaultEngine.ini里已经把NavigationSystemConfigClassName设成NullNavSysConfig，但那只
// 影响新建关卡——已有关卡(Menu/World/Editor.umap)的WorldSettings里序列化着一份
// NavigationSystemModuleConfig。所以在每个World初始化之前(UWorld::InitWorld里
// OnPreWorldInitialization先于AddNavigationSystemToWorld)给它挂一个NullNavSysConfig覆盖，
// 之后所有创建导航系统的路径(编辑器/PIE/游戏)读到的都是空配置，不会创建导航系统，也就不会
// 维护导航八叉树、不会因为增删组件/实例去刷新导航数据。
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
