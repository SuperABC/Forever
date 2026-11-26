#pragma once

#include "CoreMinimal.h"

#include "common/error.h"

class AForeverFrameworkActor;

// Core/mod抛出的C++异常的"会话边界"，设计见CoreGuard.md、CONVENTIONS.md"异常"一节。
//
// 为什么需要：UE引擎自己的代码编译时没开异常，异常一旦穿出Forever模块进到引擎的栈帧里，那些栈帧的清理代码
// 不会执行，引擎状态就坏了(编辑器里是整个编辑器崩)。所以每个"引擎直接调进来、里面会调Core"的函数体，都要用这里
// 的入口包住，让异常在离开Forever模块之前被接住。
//
// 接住之后做什么：这一局结束。Core里大量用裸new/delete，异常从中间抛出时世界停在半更新状态，不能再Tick。
// 所以FailSession把这一局标记为失败，之后所有Run都直接跳过；回到游戏线程后切回菜单关卡，错误文字交给菜单显示。
// 想要"跳过这一步继续玩"，得在更靠里的地方用GuardData接(一条脚本指令、一个里程碑的触发条件)，见error.h。
namespace CoreGuard {
	// 这一局已经因为异常结束：世界是半更新的，别再碰。
	FOREVER_API bool IsSessionFailed();

	// 一局开始/结束，AForeverFrameworkActor::BeginPlay/EndPlay调。Begin会清掉上一局的失败标记。
	void BeginSession(AForeverFrameworkActor* framework);
	void EndSession();

	// 任何线程都能调。只有第一次生效：打Error日志、标记失败、把错误文字留给菜单，然后在游戏线程上切回菜单关卡。
	// 没有正在进行的一局(菜单里)时只打日志、留文字。
	FOREVER_API void FailSession(const ErrorReport& report, const TCHAR* where);

	// 菜单取走要显示的错误文字(取一次就清空，没有则为空串)。
	FOREVER_API FString ConsumePendingMessage();

	// 当前线程套了几层Run/CORE_GUARD。只有最外面那层接异常：里层接了的话，外层会带着半更新的世界把剩下的代码跑完。
	FOREVER_API int& GuardDepth();
	struct FDepthScope {
		FDepthScope() { ++GuardDepth(); }
		~FDepthScope() { --GuardDepth(); }
		bool IsNested() const { return GuardDepth() > 1; }
		FDepthScope(const FDepthScope&) = delete;
		FDepthScope& operator=(const FDepthScope&) = delete;
	};

	// 跑一段会调Core的代码。这一局已经失败则直接跳过；抛任何异常都接住并让这一局失败。返回是否正常跑完。
	// 可以嵌套：里层不接，异常一直展开到最外层那个Run/CORE_GUARD才被接住。
	template<typename Body>
	bool Run(const TCHAR* where, Body&& body) {
		if (IsSessionFailed()) return false;
		FDepthScope depth;
		try {
			body();
			return true;
		}
		catch (...) {
			if (depth.IsNested()) throw;
			FailSession(DescribeCurrentException(), where);
			return false;
		}
	}

	// 同Run，但不管这一局是否已经失败都会跑：收尾代码(EndPlay里的释放)用。
	template<typename Body>
	bool RunAlways(const TCHAR* where, Body&& body) {
		FDepthScope depth;
		try {
			body();
			return true;
		}
		catch (...) {
			if (depth.IsNested()) throw;
			FailSession(DescribeCurrentException(), where);
			return false;
		}
	}
}

// 把一个函数体(或其中一段)整个包起来，不用改成lambda，里面的return照常用：
//     void AFoo::Tick(float deltaTime) {
//         Super::Tick(deltaTime);
//         CORE_GUARD_BEGIN("AFoo::Tick")
//         ……原来的函数体……
//         CORE_GUARD_END
//     }
// 只能用在返回void的函数里(接住异常后直接走到函数末尾)。这一局已经失败时整段跳过。
// 和CoreGuard::Run一样可以嵌套，只有最外层接异常。
#define CORE_GUARD_BEGIN(where) \
	if (!CoreGuard::IsSessionFailed()) { \
		const TCHAR* const coreGuardWhere = TEXT(where); \
		CoreGuard::FDepthScope coreGuardDepth; \
		try {
#define CORE_GUARD_END \
		} \
		catch (...) { \
			if (coreGuardDepth.IsNested()) throw; \
			CoreGuard::FailSession(DescribeCurrentException(), coreGuardWhere); \
		} \
	}

// 同上，但这一局已经失败时也照样跑：EndPlay/NativeDestruct这类收尾函数用。配CORE_GUARD_END。
#define CORE_GUARD_ALWAYS_BEGIN(where) \
	{ \
		const TCHAR* const coreGuardWhere = TEXT(where); \
		CoreGuard::FDepthScope coreGuardDepth; \
		try {
