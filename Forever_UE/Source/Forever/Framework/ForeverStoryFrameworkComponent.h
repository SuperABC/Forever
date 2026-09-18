#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverStoryFrameworkComponent.generated.h"

class Story;

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
	// 其余类型只打印类型名。Dialog分支选项没有玩家交互，默认打印第一个Option的文本。
	void BroadcastGameStart();

private:
	Story* story = nullptr;
};
