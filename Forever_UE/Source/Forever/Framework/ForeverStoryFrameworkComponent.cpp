#include "Framework/ForeverStoryFrameworkComponent.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "Framework/ForeverBuildingFrameworkComponent.h"
#include "Element/CitizenElement.h"
#include "Player/ForeverPlayerController.h"
#include "UI/SectionSpeakingWidget.h"
#include "UI/SectionOptionWidget.h"
#include "UI/MeetOptionWidget.h"

#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

#include "story/story.h"
#include "story/script.h"
#include "story/dialog.h"
#include "story/change.h"
#include "story/event.h"
#include "society/society.h"
#include "society/organization.h"
#include "society/job.h"
#include "populace/populace.h"
#include "populace/citizen.h"
#include "populace/scheduler.h"
#include "common/implement.h"

using namespace std;

namespace {
	// 从BroadcastGameStart原来的onActions lambda体搬出来的自由函数——OptionDialog()这次
	// 需要复用同一套"Dialog交给ShowDialog展示/Change转发给ApplyChange"的处理逻辑，不再只是
	// BroadcastGameStart内部的一次性lambda。不进头文件：ScriptAction是std::variant，没法
	// 前置声明，进头文件要多#include一个story/script_mod.h，没必要。
	void ProcessScriptActions(AForeverFrameworkActor* framework, const vector<ScriptAction>& actions,
		const ScriptContext& context) {
		for (const auto& action : actions) {
			if (auto dialogPtr = get_if<const Dialog*>(&action)) {
				// 阶段5 SectionSpeaking/SectionOption UI落地：不再在这里直接打印，交给
				// UForeverStoryFrameworkComponent::ShowDialog统一展示/排队，见该函数声明处
				// 注释。
				if (framework && framework->GetStoryFramework()) {
					framework->GetStoryFramework()->ShowDialog(*dialogPtr, context);
				}
			}
			else if (auto changePtr = get_if<const Change*>(&action)) {
				// 统一交给AForeverFrameworkActor::ApplyChange消费——不再在这里各自手写
				// dynamic_cast dispatch，见ForeverFrameworkActor.md"统一的Change消费入口"
				// 一节。
				if (framework) framework->ApplyChange(*changePtr, context);
				FString typeText = UTF8_TO_TCHAR((*changePtr)->GetType().data());
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("[变化] %s"), *typeText));
			}
		}
	}
}

void UForeverStoryFrameworkComponent::Init(Story* inStory) {
	story = inStory;
}

void UForeverStoryFrameworkComponent::BroadcastGameStart() {
	if (!story || !GEngine) return;

	// 现场构造一个PostImplement，供WrapScript通过Post()查询Core状态（这次唯一用到的查询是
	// "random citizen"，见Core/common/implement.md）。生命周期只需要覆盖这次广播，不用长期
	// 持有。
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Society* society = framework ? framework->GetSociety() : nullptr;
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		society,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);

	auto onActions = [framework](const vector<ScriptAction>& actions, const ScriptContext& context) {
		ProcessScriptActions(framework, actions, context);
		};

	// broadcastOne不止这次society域用得上——以后industry/traffic等其它域各自的Script要
	// 广播game_start时，同样调这个helper，不需要另起一套，所以放在if(society)之前、和
	// onActions平级定义。
	GameStartEvent event;
	auto broadcastOne = [&](Script* script) {
		if (!script) return;
		ScriptContext context;
		context.self = script;
		context.system = story->GetSystemScript();
		context.local = &event;
		onActions(script->MatchEvent(&event, context, &postImplement), context);
		};

	story->BroadcastGameStart(onActions, &postImplement);

	// 进入society域新增：Job/Organization各自持有的Script这次也要广播一次game_start，
	// 否则job_shop_saler.script/organization_shop.script里的game_start milestone永远
	// 不会被触发。
	if (society) {
		for (Organization* org : society->GetOrganizations()) {
			broadcastOne(org->GetScript());
			for (Job* job : org->GetJobs()) {
				broadcastOne(job->GetScript());
			}
		}
	}

	// 阶段5 MeetOption UI落地新增：每个市民独占持有一份Scheduler，Scheduler自己的Script
	// （schedule_empty.script）也要广播一次game_start，否则里面的add_option milestone永远
	// 不会被触发（citizen的对话选项永远加不上），见ForeverStoryFrameworkComponent.md。
	Populace* populace = framework ? framework->GetPopulace() : nullptr;
	if (populace) {
		for (Citizen* citizen : populace->GetCitizens()) {
			if (citizen && citizen->GetScheduler()) {
				broadcastOne(citizen->GetScheduler()->GetScript());
			}
		}
	}
}

void UForeverStoryFrameworkComponent::OptionDialog(const FString& name, const FString& option) {
	if (!story || !GEngine) return;

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Populace* populace = framework ? framework->GetPopulace() : nullptr;

	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		populace,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);

	string nameUtf8 = TCHAR_TO_UTF8(*name);
	string optionUtf8 = TCHAR_TO_UTF8(*option);
	OptionDialogEvent event(nameUtf8, optionUtf8);

	auto matchOne = [&](Script* script) {
		if (!script) return;
		ScriptContext context;
		context.self = script;
		context.system = story->GetSystemScript();
		context.local = &event;
		ProcessScriptActions(framework, script->MatchEvent(&event, context, &postImplement), context);
		};

	matchOne(story->GetMainScript());

	Citizen* target = populace ? populace->FindCitizenByName(nameUtf8) : nullptr;
	if (target && target->GetScheduler()) {
		matchOne(target->GetScheduler()->GetScript());
	}
}

void UForeverStoryFrameworkComponent::ApplyControlChange(const ChangeControlChange* change, const ScriptContext& context) {
	FString name = UTF8_TO_TCHAR(ToString(EvaluateExpression(change->GetName(), context)).data());

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	UForeverPopulaceFrameworkComponent* populaceFramework = framework ? framework->GetPopulaceFramework() : nullptr;
	if (!populaceFramework) return;

	ACitizenElement* target = populaceFramework->FindOrSpawnCitizenByName(name);
	if (!target) {
		debugf("Warning: ChangeControlChange target citizen not found: %s.\n", TCHAR_TO_UTF8(*name));
		return;
	}

	if (APlayerController* playerController = UGameplayStatics::GetPlayerController(GetWorld(), 0)) {
		playerController->Possess(target);
	}

	// 每次切换玩家控制权都可能是"游戏刚开始"这种建筑近处LOD(楼层/房间)还没来得及排队建完
	// 的时刻——玩家/市民会先掉到还没生成细节的地面上，等建筑加载完才落地，观感很差。这里
	// 请求冻结世界(UE时间倍率归0，物理/移动全部停摆)直到所有building的LOD切换队列清空，
	// 见UForeverBuildingFrameworkComponent::RequestFreezeUntilLodSettled。
	if (framework && framework->GetBuildingFramework()) {
		framework->GetBuildingFramework()->RequestFreezeUntilLodSettled();
	}
}

void UForeverStoryFrameworkComponent::EnqueueDialog(const Dialog* dialog, const ScriptContext& context, int32 insertIndex) {
	if (!dialog) return;

	// self/system指向长期存活的Script*/systemScript，照抄传入的context；local指向调用方
	// 栈上的运行时Event，这次调用一结束就悬垂，强制清空，见ShowDialog声明处注释。
	ScriptContext deferredContext = context;
	deferredContext.local = nullptr;

	TArray<FPendingItem> items;
	for (Section section : dialog->GetDialogs()) {
		FPendingItem item;
		item.context = deferredContext;

		if (section.IsBranch()) {
			item.bBranch = true;
			for (const Option& option : section.GetOptions()) {
				FString text = UTF8_TO_TCHAR(ToString(EvaluateExpression(option.GetOption(), context)).data());
				item.optionTexts.Add(text);
				item.options.Add(option);
			}
		}
		else {
			section.EvaluateText(context);
			auto [speaker, content, label, voice] = section.GetSpeaking();
			item.speaker = UTF8_TO_TCHAR(speaker.data());
			item.content = UTF8_TO_TCHAR(content.data());
		}

		items.Add(MoveTemp(item));
	}

	insertIndex = FMath::Clamp(insertIndex, 0, pendingQueue.Num());
	pendingQueue.Insert(items, insertIndex);
}

void UForeverStoryFrameworkComponent::ShowDialog(const Dialog* dialog, const ScriptContext& context) {
	bool wasEmpty = pendingQueue.Num() == 0;
	EnqueueDialog(dialog, context, pendingQueue.Num());
	if (wasEmpty) {
		ShowCurrentItem();
	}
}

void UForeverStoryFrameworkComponent::AdvanceDialog() {
	if (pendingQueue.Num() == 0) return;

	pendingQueue.RemoveAt(0);
	ShowCurrentItem();
}

void UForeverStoryFrameworkComponent::SelectDialogOption(int32 index) {
	if (pendingQueue.Num() == 0 || !pendingQueue[0].bBranch) return;

	// 值拷贝一份再RemoveAt——selected是对这份拷贝里的Option取引用，RemoveAt(0)之后原数组
	// 元素的生命周期就跟这次调用无关了。
	FPendingItem current = pendingQueue[0];
	if (!current.options.IsValidIndex(index)) return;

	pendingQueue.RemoveAt(0);

	const Option& selected = current.options[index];

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (framework) {
		for (const Change* change : selected.GetChanges()) {
			framework->ApplyChange(change, current.context);
		}
	}

	// 选中后追加的对话应该紧接着这个分支之后立刻播放，插到队头而不是排到队尾；多个嵌套
	// Dialog之间要保持相对顺序，insertCursor随每次插入新增的条目数往后走。
	int32 insertCursor = 0;
	for (Dialog* nested : selected.GetDialogs()) {
		int32 before = pendingQueue.Num();
		EnqueueDialog(nested, current.context, insertCursor);
		insertCursor += pendingQueue.Num() - before;
	}

	ShowCurrentItem();
}

void UForeverStoryFrameworkComponent::ShowCurrentItem() {
	AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!playerController) return;

	USectionSpeakingWidget* speakingWidget = playerController->GetSectionSpeakingWidget();
	USectionOptionWidget* optionWidget = playerController->GetSectionOptionWidget();
	UMeetOptionWidget* meetOptionWidget = playerController->GetMeetOptionWidget();

	if (pendingQueue.Num() == 0) {
		if (speakingWidget) speakingWidget->HideLine();
		if (optionWidget) optionWidget->HideOptions();
		// 对话结束，重新露出MeetOption（Visible是AddToViewport()之后的默认可见状态）——
		// 见下面"对话展示期间"分支里把它藏起来的说明。
		if (meetOptionWidget) meetOptionWidget->SetVisibility(ESlateVisibility::Visible);

		// 对话播完，恢复正常的游戏内输入。光把bShowMouseCursor/SetInputMode改回去还不够——
		// UIOnly期间点SectionButton/ClickCatcher这次鼠标点击本身会把Slate的键盘/鼠标焦点
		// 转移到那个Button上，SetInputMode(GameOnly)并不会主动抢回来，停留在UI焦点上的话
		// 即使鼠标已经隐藏，Look这个Enhanced Input Action也收不到鼠标移动事件（实测复现：
		// 对话关闭后鼠标不消失、也没法转视角）。必须再调用一次SetFocusToGameViewport()把
		// 焦点抢回游戏视口。
		playerController->bShowMouseCursor = false;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(playerController);
		UWidgetBlueprintLibrary::SetFocusToGameViewport();
		return;
	}

	// 同时把MeetOption藏起来——对话（不管是台词还是分支）展示期间，"靠近市民能问什么"这个
	// 列表不应该跟剧情对话同屏抢注意力/抢点击，见老蓝图SectionButton.SelectOption分支里
	// "选完把MeetOption.SetVisibility(Visible)调回来"那段逻辑（.dump/SelectionButton.txt）。
	// 用SetVisibility而不是加一个MeetOptionWidget自己的Show/Hide方法——UUserWidget的
	// SetVisibility本来就是public UFUNCTION，不需要再包一层。
	if (meetOptionWidget) meetOptionWidget->SetVisibility(ESlateVisibility::Collapsed);

	const FPendingItem& item = pendingQueue[0];
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());

	UWidget* widgetToFocus = nullptr;
	if (item.bBranch) {
		if (speakingWidget) speakingWidget->HideLine();
		if (optionWidget) optionWidget->ShowOptions(framework, item.optionTexts);
		widgetToFocus = optionWidget;
	}
	else {
		if (optionWidget) optionWidget->HideOptions();
		if (speakingWidget) speakingWidget->ShowLine(framework, item.speaker, item.content);
		widgetToFocus = speakingWidget;
	}

	// 对话展示期间切UIOnly（仿照老工程SectionButton点击后调SetInputMode_GameOnly、隐含
	// 之前是"游戏动作完全不响应"这个状态，见.dump/SelectionButton.txt）——UIOnly下Pawn的
	// Enhanced Input完全收不到任何输入，Jump/Move/Look/MeetOption的F/滚轮全都天然不会
	// 误触发，不需要像"空格键同时绑Jump和推进对话"那样一个个手动判断"对话是不是在展示"
	// （之前踩过这个坑：GameAndUI下Jump还是会响应，按空格推进对话的同时角色会跳一下）。
	// ClickCatcher/SectionButton的鼠标点击在UIOnly下依然正常工作，跟GameAndUI没有区别；
	// 唯一的代价是SectionSpeaking的空格键推进不能再走Enhanced Input（Pawn收不到输入），
	// 改成SectionSpeaking自己的NativeOnKeyDown原生响应键盘事件，所以这里必须把
	// widgetToFocus指定成当前实际显示的那个Widget，它才能拿到键盘焦点，见
	// USectionSpeakingWidget.h类注释。不判断"是不是刚从空队列进入"，每次都设一遍——
	// 重复设置同一个输入模式/显示鼠标状态没有副作用，比额外维护一个"上一次状态"标记简单。
	playerController->bShowMouseCursor = true;
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(playerController, widgetToFocus, EMouseLockMode::DoNotLock, false);
}
