#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"

#include "story/dialog.h"

#include "ForeverStoryFrameworkComponent.generated.h"

class Story;
class Change;
class ChangeControlChange;
class StartPuzzleChange;
struct ScriptContext;

// 阶段4 Story落地：这次把Script->Milestone->Event/Dialog/Change架构跑通到"游戏开始广播
// GameStartEvent，匹配出的Dialog交给SectionSpeaking/SectionOption两个UI展示、Change转发给
// ApplyChange"这一步。详见ForeverStoryFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverStoryFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	// 由AForeverFrameworkActor在Story::Init()跑完后调用一次。story生命周期由调用方持有。
	void Init(Story* inStory);

	// 广播一次GameStartEvent，把匹配出的Dialog交给ShowDialog展示、Change转发给ApplyChange。
	// 内部会现场构造一个PostImplement（见Core/common/implement.h）传给Story::BroadcastGameStart，
	// 供WrapScript查询Core状态（如随机挑一个citizen）。**这次一起把Society下所有
	// Organization/Job各自持有的Script、以及每个市民独占持有的Scheduler Script也广播一次
	// game_start**（不新开函数，直接在这个函数体里遍历，复用同一份处理结果的onActions
	// 逻辑）——否则它们各自的game_start milestone永远不会被触发，见.cpp实现。
	void BroadcastGameStart();

	// 玩家点击MeetOption选项时调用：构造一个OptionDialogEvent，依次匹配主线剧情Script
	// （如果配置了）和目标市民的Scheduler Script，复用BroadcastGameStart同一套Dialog展示/
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

	// StartPuzzleChange的实际执行：找到AForeverPlayerController持有的PuzzleWidget，让它
	// 开始一局指定类型的小游戏。不在Core::Story::ApplyChange里处理（Core层不知道UMG Widget
	// 的存在），只能在这一层做，跟ApplyControlChange同一个理由。
	// @change: 待执行的开始小游戏变化
	// @context: 变量路由上下文，用于求值change->GetPuzzle()
	void ApplyStartPuzzle(const StartPuzzleChange* change, const ScriptContext& context);

	// 阶段5 SectionSpeaking/SectionOption UI落地：把ProcessScriptActions匹配出的一个Dialog
	// 交给这个函数展示，取代原来"直接打印到屏幕左上角"的占位做法（Dialog分支选项原来"没有
	// 玩家交互，默认打印第一个Option"的限制这次一并解除，见ForeverStoryFrameworkComponent.md）。
	// Dialog::GetDialogs()拿到的Section拷贝在这里被逐个求值/展平进pendingQueue：普通台词
	// 求值成speaker/content字符串；分支选项求值每个Option::GetOption()的显示文本，原始
	// Option（连同它引用的嵌套dialogs/changes）整个存一份供选中后使用。求值动作必须在这次
	// 调用里同步完成——context.local指向调用方栈上的运行时Event（BroadcastGameStart的
	// GameStartEvent/OptionDialog的OptionDialogEvent），函数返回后就是悬垂指针，不能留到玩家
	// 点击的那一刻（可能是任意多帧之后）才用（这个悬垂风险的完整解释见BroadcastGameStart
	// 原本的实现注释、Story::BroadcastGameStart声明处的"用回调而不是直接返回"说明）。玩家
	// 选中分支选项后的Change/嵌套Dialog改用一份"去掉local"的context（self/system不变——
	// 它们指向长期存活的Script*/systemScript，只有local才是这次要规避的悬垂来源），代价是
	// 分支选项自己的嵌套内容没法再引用local.前缀变量（比如触发这次对话的具体
	// OptionDialogEvent.option文本）——这次范围内没有用到这个能力，真需要时得把要用的值提前
	// 求值烘焙进某个Script的变量池，而不是设法延长Event本身的生命周期。
	// 队列当前为空（没有对话正在展示）时处理完立刻显示第一项；否则追加到队尾，等当前对话
	// 播完再显示——先到先播完，不打断/不并发（游戏开局多个Script同时广播game_start、都命中
	// 对话milestone时，会排队一条条弹出，不会互相覆盖）。
	// @dialog: 待展示的对话（引用，不持有所有权，本体挂在触发它的Milestone上）
	// @context: 变量路由上下文，只在这次调用同步使用，见上面的说明
	void ShowDialog(const Dialog* dialog, const ScriptContext& context);

	// SectionSpeaking的ClickCatcher点击后调用（见USectionSpeakingWidget::HandleClicked）：
	// 关闭当前这一条台词，推进到队列里的下一项。队列为空时no-op。
	void AdvanceDialog();

	// SectionButton点击后调用（见USectionButtonWidget::HandleClicked）：index是当前分支
	// Section::GetOptions()里被选中的那个下标——执行它的GetChanges()（转发给
	// AForeverFrameworkActor::ApplyChange），并把它的GetDialogs()展平插到队列最前面（选中后
	// 追加的对话应该紧接着当前分支之后立刻播放，不是排到队尾），然后推进显示。当前队首不是
	// 分支、或index越界时no-op（防御性写法，正常UI流程不会出现这种调用）。
	// @index: Section::GetOptions()数组下标
	void SelectDialogOption(int32 index);

private:
	// 队列里的一项：要么是一条已求值好的台词，要么是一个分支（已求值好的选项文本+原始Option
	// 拷贝，供SelectDialogOption执行）。不用TVariant——这个结构体只在这个.cpp内部私有使用，
	// 没有反射/蓝图暴露需求，一个bool+两组字段够用，不需要额外引入TVariant的复杂度。
	struct FPendingItem {
		bool bBranch = false;

		// bBranch为false时有效
		FString speaker;
		FString content;

		// bBranch为true时有效，两个数组下标一一对应
		TArray<FString> optionTexts;
		TArray<Option> options;

		// 求值/执行这一项时用的上下文——self/system照抄自ShowDialog传入的原始context（指向
		// 长期存活的Script*/systemScript），local强制清空，见ShowDialog声明处注释。
		ScriptContext context;
	};

	// 把dialog->GetDialogs()展平成若干FPendingItem，插入pendingQueue的insertIndex位置——
	// 求值动作在这里同步完成，见ShowDialog声明处注释。
	// @insertIndex: 0表示插到队头（SelectDialogOption的嵌套对话用），
	// pendingQueue.Num()表示追加到队尾（ShowDialog用）
	void EnqueueDialog(const Dialog* dialog, const ScriptContext& context, int32 insertIndex);

	// 显示pendingQueue最前面那一项（不弹出——弹出分别由AdvanceDialog/SelectDialogOption做，
	// 因为要弹出"刚被处理完的那一项"而不是"即将显示的下一项"，语义上放在调用方更清楚）；
	// 队列为空则把SectionSpeaking/SectionOption两个Widget都隐藏。找不到
	// AForeverPlayerController/对应Widget时静默no-op（和其它UI调用点一致的防御写法）。
	void ShowCurrentItem();

	Story* story = nullptr;

	TArray<FPendingItem> pendingQueue;
};
