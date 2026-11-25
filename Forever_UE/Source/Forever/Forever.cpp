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

#include "common/utility.h"

// Core/Dependence/各mod里不带UE的代码用debugf打的日志，统一进这个类别(Forever.log里搜LogForeverCore)。
DEFINE_LOG_CATEGORY_STATIC(LogForeverCore, Log, All);

namespace {
	// debugf的输出函数(见Dependence/common/utility.md)：mod dll加载时也会拿到它(ModLoader经BindModHost交过去)，
	// 所以可能在任意线程、从任意dll被调用——只做转码和UE_LOG(线程安全)，不碰游戏对象。
	void ForeverDebugSink(DebugLevel level, const char* text) {
		FString line = UTF8_TO_TCHAR(text ? text : "");
		line.TrimEndInline(); // 调用方习惯以换行结尾，UE_LOG自己换行
		if (line.IsEmpty()) return;
		switch (level) {
		case DebugLevel::Error: UE_LOG(LogForeverCore, Error, TEXT("%s"), *line); break;
		case DebugLevel::Warning: UE_LOG(LogForeverCore, Warning, TEXT("%s"), *line); break;
		default: UE_LOG(LogForeverCore, Log, TEXT("%s"), *line); break;
		}
	}
}

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
		// 最先装：之后任何Core代码(包括加载mod)打的日志都进UE日志。
		SetDebugSink(&ForeverDebugSink);
		preWorldInitHandle = FWorldDelegates::OnPreWorldInitialization.AddStatic(&FForeverGameModule::DisableNavigation);
		// 烘焙/打包(cook、pak等commandlet)里游戏线程根本不跑游戏帧，GFrameCounter一直是0，看门狗会每2秒误报一次(栈也没符号)，所以只在真正运行游戏时开。
		if (!IsRunningCommandlet()) watchdog = MakeUnique<FForeverHitchWatchdog>();
	}

	virtual void ShutdownModule() override {
		SetDebugSink(nullptr);
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
