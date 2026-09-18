#include "Framework/ForeverStoryFrameworkComponent.h"

#include "Engine/Engine.h"

#include "story/story.h"
#include "story/script.h"
#include "story/dialog.h"
#include "story/change.h"

using namespace std;

void UForeverStoryFrameworkComponent::Init(Story* inStory) {
	story = inStory;
}

void UForeverStoryFrameworkComponent::BroadcastGameStart() {
	if (!story || !GEngine) return;

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
				story->ApplyChange(*changePtr, context);
				FString typeText = UTF8_TO_TCHAR((*changePtr)->GetType().data());
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("[变化] %s"), *typeText));
			}
		}
		});
}
