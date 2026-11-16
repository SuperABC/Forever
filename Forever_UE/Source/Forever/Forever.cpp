// Copyright Epic Games, Inc. All Rights Reserved.

#include "Forever.h"
#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "AI/NavigationSystemConfig.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "CoreGlobals.h"

// 卡顿看门狗(开发期排查用)：游戏线程超过2秒没推进一帧，就把游戏线程当前的调用栈打到日志里(之后每2秒重复一次，直到恢复)。
// 只读GFrameCounter和抓栈，不碰游戏对象。栈要符号：开发版编辑器带PDB即可。
class FForeverHitchWatchdog : public FRunnable {
public:
	FForeverHitchWatchdog() { thread = FRunnableThread::Create(this, TEXT("ForeverHitchWatchdog"), 0, TPri_BelowNormal); }
	virtual ~FForeverHitchWatchdog() override {
		bStop = true;
		if (thread) { thread->WaitForCompletion(); delete thread; }
	}

	virtual uint32 Run() override {
		uint64 lastFrame = GFrameCounter;
		double lastChange = FPlatformTime::Seconds();
		double lastReport = 0.0;
		while (!bStop) {
			FPlatformProcess::Sleep(0.25f);
			const uint64 frame = GFrameCounter;
			const double now = FPlatformTime::Seconds();
			if (frame != lastFrame) { lastFrame = frame; lastChange = now; lastReport = 0.0; continue; }
			if (now - lastChange < 2.0 || now - lastReport < 2.0) continue;
			lastReport = now;
			uint64 backtrace[48] = {};
			const uint32 depth = FPlatformStackWalk::CaptureThreadStackBackTrace(GGameThreadId, backtrace, 48);
			FString text = FString::Printf(TEXT("HitchWatchdog: 游戏线程已 %.1f 秒没有推进帧(frame=%llu)，当前调用栈：\n"), now - lastChange, frame);
			for (uint32 i = 0; i < depth; i++) {
				FProgramCounterSymbolInfo info;
				FPlatformStackWalk::ProgramCounterToSymbolInfo(backtrace[i], info);
				text += FString::Printf(TEXT("  #%u %hs  (%hs:%u)\n"), i, info.FunctionName, info.Filename, info.LineNumber);
			}
			UE_LOG(LogTemp, Warning, TEXT("%s"), *text);
		}
		return 0;
	}
	virtual void Stop() override { bStop = true; }

private:
	FRunnableThread* thread = nullptr;
	TAtomic<bool> bStop{ false };
};

// 在每个World初始化之前关掉UE自带的导航系统(项目用Core导航图)，原因和时序见Forever.md。
class FForeverGameModule : public FDefaultGameModuleImpl {
public:
	virtual void StartupModule() override {
		preWorldInitHandle = FWorldDelegates::OnPreWorldInitialization.AddStatic(&FForeverGameModule::DisableNavigation);
		watchdog = MakeUnique<FForeverHitchWatchdog>();
	}

	virtual void ShutdownModule() override {
		FWorldDelegates::OnPreWorldInitialization.Remove(preWorldInitHandle);
		watchdog.Reset();
	}

private:
	static void DisableNavigation(UWorld* world, const UWorld::InitializationValues) {
		AWorldSettings* worldSettings = world ? world->GetWorldSettings() : nullptr;
		if (!worldSettings || worldSettings->GetNavigationSystemConfigOverride()) return;
		worldSettings->SetNavigationSystemConfigOverride(NewObject<UNullNavSysConfig>(worldSettings, NAME_None, RF_Transient));
	}

	FDelegateHandle preWorldInitHandle;
	TUniquePtr<FForeverHitchWatchdog> watchdog;
};

IMPLEMENT_PRIMARY_GAME_MODULE( FForeverGameModule, Forever, "Forever" );
