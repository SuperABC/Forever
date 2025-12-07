#include "story.h"
#include "change.h"
#include "json.h"
#include "error.h"

#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <unordered_map>


using namespace std;

Story::Story() {
	eventFactory.reset(new EventFactory());
	changeFactory.reset(new ChangeFactory());
}

Story::~Story() {

}

void Story::InitEvents() {
	eventFactory->RegisterEvent(GameStartEvent::GetId(), []() { return make_unique<GameStartEvent>(); });
	eventFactory->RegisterEvent(OptionDialogEvent::GetId(), []() { return make_unique<OptionDialogEvent>(); });

	HMODULE modHandle = LoadLibraryA(REPLACE_PATH("Mod.dll"));
	if (modHandle) {
		modHandles.push_back(modHandle);
		debugf("Mod dll loaded successfully.\n");

		RegisterModEventsFunc registerFunc = (RegisterModEventsFunc)GetProcAddress(modHandle, "RegisterModEvents");
		if (registerFunc) {
			registerFunc(eventFactory.get());
		}
		else {
			debugf("Incorrect dll content.");
		}
	}
	else {
		debugf("Failed to load mod.dll.");
	}

#ifdef MOD_TEST
	auto eventList = { "game_start", "option_dialog", "mod"};
	for (const auto& eventId : eventList) {
		if (eventFactory->CheckRegistered(eventId)) {
			auto event = eventFactory->CreateEvent(eventId);
			debugf(("Created event: " + event->GetName() + " (ID: " + eventId + ")\n").data());
		}
		else {
			debugf("Event not registered: %s\n", eventId);
		}
	}
#endif // MOD_TEST
}

void Story::InitChanges() {
	changeFactory->RegisterChange(SetValueChange::GetId(), []() { return make_unique<SetValueChange>(); });
	changeFactory->RegisterChange(RemoveValueChange::GetId(), []() { return make_unique<RemoveValueChange>(); });

	HMODULE modHandle = LoadLibraryA(REPLACE_PATH("Mod.dll"));
	if (modHandle) {
		modHandles.push_back(modHandle);
		debugf("Mod dll loaded successfully.\n");

		RegisterModChangesFunc registerFunc = (RegisterModChangesFunc)GetProcAddress(modHandle, "RegisterModChanges");
		if (registerFunc) {
			registerFunc(changeFactory.get());
		}
		else {
			debugf("Incorrect dll content.");
		}
	}
	else {
		debugf("Failed to load mod.dll.");
	}

#ifdef MOD_TEST
	auto changeList = { "set_value", "remove_value", "mod" };
	for (const auto& changeId : changeList) {
		if (changeFactory->CheckRegistered(changeId)) {
			auto change = changeFactory->CreateChange(changeId);
			debugf(("Created change: " + change->GetName() + " (ID: " + changeId + ")\n").data());
		}
		else {
			debugf("Change not registered: %s\n", changeId);
		}
	}
#endif // MOD_TEST
}

void Story::Init() {
	InitVariables();

	script.reset(new Script());
}

void Story::Print() {
	cout << "全局变量数量 " << variables.size() << endl;
	for (auto value : variables) {
		cout << value.first << ": " << ToString(value.second) << endl;
	}
}

void Story::ReadStory(string path) {
	script->ReadScript(path, eventFactory, changeFactory);
}

void Story::ApplyChange(shared_ptr<Change> change) {
	auto type = change->GetType();
	if(type == "set_value") {
		auto obj = dynamic_pointer_cast<SetValueChange>(change);
		if (obj->GetVariable().substr(0, 7) == "system.") {
			return;
		}
		Condition condition;
		condition.ParseCondition(obj->GetValue());
		variables[obj->GetVariable()] = condition.EvaluateValue([this](string name) -> ValueType {
			return this->GetValue(name);
			});
	}
	else if(type == "remove_value") {
		auto obj = dynamic_pointer_cast<RemoveValueChange>(change);
		if (obj->GetVariable().substr(0, 7) == "system.") {
			return;
		}
		variables.erase(obj->GetVariable());
	}
}

void Story::SaveStory(string path) {

}

void Story::LoadStory(string path) {

}

bool Story::JudgeCondition(Condition& condition) {
	return condition.EvaluateBool([this](string name) -> ValueType {
		return this->GetValue(name);
		});
}

pair<vector<Dialog>, vector<shared_ptr<Change>>> Story::MatchEvent(shared_ptr<Event> event) {
	return script->MatchEvent(event, this);
}

string Story::ReplaceContent(const string& content) {
	std::string result;
	size_t pos = 0;
	size_t lastPos = 0;

	while ((pos = content.find("$$", lastPos)) != std::string::npos) {
		result.append(content, lastPos, pos - lastPos);

		size_t varStart = pos + 2;
		size_t varEnd = varStart;
		while (varEnd < content.length() && IsIdentifierChar(content[varEnd])) {
			varEnd++;
		}

		std::string varName = content.substr(varStart, varEnd - varStart);
		if (!varName.empty()) {
			auto it = variables.find(varName);
			if (it != variables.end()) {
				result += ToString(it->second);
			}
			else {
				result.append(content, pos, varEnd - pos);
			}
		}
		else {
			result.append(content, pos, 2);
		}
		lastPos = varEnd;
	}
	result.append(content, lastPos, content.length() - lastPos);

	return result;
}

void Story::InitVariables() {
    variables["system.health_status"] = "healthy";
}

ValueType Story::GetValue(const std::string& name) {
	auto it = variables.find(name);
	if (it != variables.end()) {
		return it->second;
	}
	return 0; // 默认返回0
}

void Story::SetValue(const std::string& name, ValueType value) {
	variables[name] = value;
}


