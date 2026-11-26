#include "Framework/CoreGuard.h"

#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeLock.h"

#include "Framework/ForeverFrameworkActor.h"
#include "UI/FadeSubsystem.h"

#include <atomic>

DEFINE_LOG_CATEGORY_STATIC(LogForeverGuard, Log, All);

namespace {
	// 只存文字和一个弱引用，不存任何Core/mod指针：这些静态量活得比关卡久(CONVENTIONS.md"mod dll的生命周期")。
	std::atomic<bool> sessionFailed{ false };
	FCriticalSection messageLock;
	FString pendingMessage;
	TWeakObjectPtr<AForeverFrameworkActor> sessionFramework;

	const TCHAR* KindText(ErrorKind kind) {
		switch (kind) {
		case ErrorKind::Data: return TEXT("数据错误");
		case ErrorKind::Mod: return TEXT("mod错误");
		default: return TEXT("内部错误");
		}
	}

	// 游戏线程：把这一局收掉，回菜单。
	void ReturnToMenu() {
		check(IsInGameThread());
		AForeverFrameworkActor* framework = sessionFramework.Get();
		if (!framework) return; // 没有正在进行的一局，或者关卡已经在销毁
		UWorld* world = framework->GetWorld();
		if (!world || world->bIsTearingDown) return;

		// 开局生成世界时黑屏遮罩一直等"进度满格"才撤；这一局不会满格了，不撤的话回到菜单还是黑的。
		if (UGameInstance* gameInstance = world->GetGameInstance()) {
			if (UFadeSubsystem* fade = gameInstance->GetSubsystem<UFadeSubsystem>()) fade->Dismiss();
		}
		UGameplayStatics::SetGamePaused(world, false);
		// OpenLevel销毁当前世界：AForeverFrameworkActor::EndPlay同步释放Core各域并卸载mod dll。
		UGameplayStatics::OpenLevel(world, FName(TEXT("/Game/Levels/Menu")));
	}
}

int& CoreGuard::GuardDepth() {
	thread_local int depth = 0;
	return depth;
}

bool CoreGuard::IsSessionFailed() {
	return sessionFailed.load(std::memory_order_acquire);
}

void CoreGuard::BeginSession(AForeverFrameworkActor* framework) {
	check(IsInGameThread());
	sessionFramework = framework;
	sessionFailed.store(false, std::memory_order_release);
}

void CoreGuard::EndSession() {
	check(IsInGameThread());
	sessionFramework.Reset();
	// 失败标记留着：这一局收尾期间还在排队的调用继续跳过，下一局BeginSession再清。
}

void CoreGuard::FailSession(const ErrorReport& report, const TCHAR* where) {
	const FString text = UTF8_TO_TCHAR(report.ToText().c_str());
	bool expected = false;
	if (!sessionFailed.compare_exchange_strong(expected, true)) {
		// 这一局已经在收尾：后面再出的异常多半是前一个的连锁反应，只记一笔。
		UE_LOG(LogForeverGuard, Warning, TEXT("这一局已经因异常结束，%s 又接到一个异常：\n%s"), where, *text);
		return;
	}
	UE_LOG(LogForeverGuard, Error, TEXT("这一局因异常结束（在 %s 接住）：\n%s"), where, *text);

	FString message = FString::Printf(TEXT("上一局因%s结束：%s"), KindText(report.kind), UTF8_TO_TCHAR(report.message.c_str()));
	if (!report.mod.empty()) message += FString::Printf(TEXT("\n出错的mod：%s"), UTF8_TO_TCHAR(report.mod.c_str()));
	message += TEXT("\n详细信息见日志(搜 LogForeverGuard)。");
	{
		FScopeLock lock(&messageLock);
		pendingMessage = MoveTemp(message);
	}

	// 可能是在线程池里、也可能是在游戏线程某个很深的调用栈里接到的：都排到游戏线程稍后再切关卡。
	AsyncTask(ENamedThreads::GameThread, []() { ReturnToMenu(); });
}

FString CoreGuard::ConsumePendingMessage() {
	FScopeLock lock(&messageLock);
	FString message = MoveTemp(pendingMessage);
	pendingMessage.Reset();
	return message;
}
