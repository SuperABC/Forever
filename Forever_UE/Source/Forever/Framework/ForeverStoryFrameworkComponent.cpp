#include "Framework/ForeverStoryFrameworkComponent.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "Element/CitizenElement.h"

#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

#include "story/story.h"
#include "story/script.h"
#include "story/dialog.h"
#include "story/change.h"
#include "story/event.h"
#include "society/society.h"
#include "society/organization.h"
#include "society/job.h"
#include "common/implement.h"

using namespace std;

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

	auto onActions = [this, framework](const vector<ScriptAction>& actions, const ScriptContext& context) {
		for (const auto& action : actions) {
			if (auto dialogPtr = get_if<const Dialog*>(&action)) {
				for (Section section : (*dialogPtr)->GetDialogs()) {
					if (section.IsBranch()) {
						// 没有玩家交互，默认选中第一个选项，见ForeverStoryFrameworkComponent.md
						auto options = section.GetOptions();
						if (!options.empty()) {
							FString text = UTF8_TO_TCHAR(ToString(EvaluateExpression(options[0].GetOption(), context)).data());
							GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("[对话选项] %s"), *text));
						}
					}
					else {
						section.EvaluateText(context);
						auto [speaker, content, label, voice] = section.GetSpeaking();
						FString speakerText = UTF8_TO_TCHAR(speaker.data());
						FString contentText = UTF8_TO_TCHAR(content.data());
						FString line = speakerText.IsEmpty() ? contentText : FString::Printf(TEXT("%s：%s"), *speakerText, *contentText);
						GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, line);
					}
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
}
