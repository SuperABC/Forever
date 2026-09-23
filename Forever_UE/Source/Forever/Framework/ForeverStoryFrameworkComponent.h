#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverStoryFrameworkComponent.generated.h"

class Story;
class Change;
class ChangeControlChange;
struct ScriptContext;

// 阶段4 Story落地：这次只把Script->Milestone->Event/Dialog/Change架构跑通到"游戏开始广播
// GameStartEvent，匹配出的Dialog/SetValueChange打印在屏幕左上角"这一步，UI呈现/剧情内容
// 后续阶段再深化。详见ForeverStoryFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverStoryFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	// 由AForeverFrameworkActor在Story::Init()跑完后调用一次。story生命周期由调用方持有。
	void Init(Story* inStory);

	// 广播一次GameStartEvent，把匹配出的Dialog/Change打印到屏幕左上角（GEngine::
	// AddOnScreenDebugMessage）。Change只有SetValueChange真正执行(Story::ApplyChange)，
	// ChangeControlChange/DebugPrintChange由这个组件自己拦截处理（见.cpp——前者需要
	// Actor/Controller，后者需要GEngine，Core层都不认识），其余类型只打印类型名。Dialog分支
	// 选项没有玩家交互，默认打印第一个Option的文本。内部会现场构造一个PostImplement
	// （见Core/common/implement.h）传给Story::BroadcastGameStart，供WrapScript查询Core状态
	// （如随机挑一个citizen）。**这次一起把Society下所有Organization/Job各自持有的Script
	// 也广播一次game_start**（不新开函数，直接在这个函数体里遍历，复用同一份处理结果的
	// onActions逻辑）——否则它们各自的game_start milestone永远不会被触发，见.cpp实现。
	void BroadcastGameStart();

	// 玩家点击MeetOption选项时调用：构造一个OptionDialogEvent，依次匹配主线剧情Script
	// （如果配置了）和目标市民的Scheduler Script，复用BroadcastGameStart同一套Dialog打印/
	// Change转发逻辑（见.cpp里的ProcessScriptActions）。不实现GlobalDialog（只匹配主线
	// 剧情的全局选项分支）——这次MeetOption UI落地范围明确不做。
	// @name: 对话目标（市民）姓名字面量
	// @option: 被点击的选项文本字面量
	void OptionDialog(const FString& name, const FString& option);

	// ChangeControlChange的实际执行：把玩家操控权切换到指定姓名的市民身上。不在
	// Story::ApplyChange里处理（Core层不认识Actor/Controller），只能在这一层做。这次改成
	// public——AForeverFrameworkActor::ApplyChange统一收口所有Change的消费入口后，
	// ChangeControlChange分支需要从那里转发调用到这里，见ForeverFrameworkActor.md。
	// @change: 待执行的切换控制变化
	// @context: 变量路由上下文，用于求值change->GetName()
	void ApplyControlChange(const ChangeControlChange* change, const ScriptContext& context);

private:
	Story* story = nullptr;
};
