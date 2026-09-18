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
	// ChangeControlChange由这个组件自己拦截处理（见.cpp），其余类型只打印类型名。Dialog分支
	// 选项没有玩家交互，默认打印第一个Option的文本。内部会现场构造一个PostImplement
	// （见Core/common/implement.h）传给Story::BroadcastGameStart，供WrapScript查询Core状态
	// （如随机挑一个citizen）。
	void BroadcastGameStart();

private:
	// ChangeControlChange的实际执行：把玩家操控权切换到指定姓名的市民身上。不在
	// Story::ApplyChange里处理（Core层不认识Actor/Controller），只能在这一层做。
	// @change: 待执行的切换控制变化
	// @context: 变量路由上下文，用于求值change->GetName()
	void ApplyControlChange(const ChangeControlChange* change, const ScriptContext& context);

	Story* story = nullptr;
};
