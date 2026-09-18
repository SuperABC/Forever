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
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);

	story->BroadcastGameStart([this](const vector<ScriptAction>& actions, const ScriptContext& context) {
		for (const auto& action : actions) {
			if (auto dialogPtr = get_if<const Dialog*>(&action)) {
				for (Section section : (*dialogPtr)->GetDialogs()) {
					if (section.IsBranch()) {
						// 没有玩家交互，默认选中第一个选项，见ForeverStoryFrameworkComponent.md
						auto options = section.GetOptions();
						if (!options.empty()) {
							FString text = UTF8_TO_TCHAR(ToString(options[0].GetOption().EvaluateValue(context)).data());
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
				if (auto controlChange = dynamic_cast<const ChangeControlChange*>(*changePtr)) {
					ApplyControlChange(controlChange, context);
				}
				else {
					story->ApplyChange(*changePtr, context);
				}
				FString typeText = UTF8_TO_TCHAR((*changePtr)->GetType().data());
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("[变化] %s"), *typeText));
			}
		}
		}, &postImplement);
}

void UForeverStoryFrameworkComponent::ApplyControlChange(const ChangeControlChange* change, const ScriptContext& context) {
	FString name = UTF8_TO_TCHAR(ToString(change->GetName().EvaluateValue(context)).data());

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
