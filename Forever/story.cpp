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
	auto eventList = { "mod"};
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
	auto changeList = { "mod" };
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

unique_ptr<EventFactory>& Story::GetEventFactory() {
	return eventFactory;
}

unique_ptr<ChangeFactory>& Story::GetChangeFactory() {
	return changeFactory;
}

void Story::ReadConfigs(string path) const {
	if (!filesystem::exists(path)) {
		THROW_EXCEPTION(IOException, "Path does not exist: " + path + ".\n");
	}

	JsonReader reader;
	JsonValue root;

	ifstream fin(path);
	if (!fin.is_open()) {
		THROW_EXCEPTION(IOException, "Failed to open file: " + path + ".\n");
	}
	if (reader.Parse(fin, root)) {
		for (auto event : root["mods"]["event"]) {
			eventFactory->SetConfig(event.AsString(), true);
		}
		for (auto change : root["mods"]["change"]) {
			changeFactory->SetConfig(change.AsString(), true);
		}

	}
	else {
		fin.close();
		THROW_EXCEPTION(JsonFormatException, "Json syntax error: " + reader.GetErrorMessages() + ".\n");
	}
	fin.close();
}

void Story::Init(unique_ptr<Populace>& populace) {
	InitVariables(populace);

	script.reset(new Script());
}

void Story::Destroy() {

}

void Story::Tick(int day, int hour, int min, int sec) {

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
		variables[obj->GetVariable()] = condition.EvaluateValue([this](string name) -> pair<bool, ValueType> {
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
	return condition.EvaluateBool([this](string name) -> pair<bool, ValueType> {
		return this->GetValue(name);
		});
}

pair<vector<Dialog>, vector<shared_ptr<Change>>> Story::MatchEvent(shared_ptr<Event> event) {
	return script->MatchEvent(event, this);
}

void Story::InitVariables(unique_ptr<Populace>& populace) {
	variables["system.time.year"] = populace->GetTime().GetYear();

	variables["player.name"] = "玩家";
    variables["player.health"] = "健康";

}

pair<bool, ValueType> Story::GetValue(const string& name) {
	auto it = variables.find(name);
	if (it != variables.end()) {
		return { true, it->second };
	}
	return { false, 0 };
}

void Story::SetValue(const string& name, ValueType value) {
	variables[name] = value;
}


