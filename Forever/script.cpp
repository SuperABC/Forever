#include "script.h"
#include "change.h"
#include "json.h"
#include "error.h"

#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <unordered_map>


using namespace std;

Script::Script() {
	eventFactory.reset(new EventFactory());
	changeFactory.reset(new ChangeFactory());
}

Script::~Script() {

}

void Script::InitEvents() {
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

void Script::InitChanges() {
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

void Script::Init() {
	InitVariables();
}

void Script::Print() {
	cout << "活动里程碑数量 " << actives.size() << endl;
	for (auto active : actives) {
		cout << active->content.GetName() << ": " << active->content.GetGoal() << endl;
	}

	cout << "全局变量数量 " << variables.size() << endl;
	for (auto value : variables) {
		cout << value.first << ": " << ToString(value.second) << endl;
	}
}

void Script::ReadScript(string path) {
    if (!filesystem::exists(path)) {
        THROW_EXCEPTION(IOException, "Path does not exist: " + path + ".\n");
    }

	Json::Reader reader;
	Json::Value root;

	ifstream fin(path);
	if (!fin.is_open()) {
        THROW_EXCEPTION(IOException, "Failed to open file: " + path + ".\n");
	}
	if (reader.parse(fin, root)) {
		unordered_map<string, int> hash;
		hash["game_finish"] = -1;
		hash["game_fail"] = -2;

		for (auto milestone : root) {
			Milestone content(
				milestone["milestone"].asString(),
				BuildEvent(milestone["triggers"]),
				milestone["visible"].asBool(),
				BuildCondition(milestone["drop"].asString()),
				milestone["description"].asString(),
				milestone["goal"].asString(),
				BuildDialogs(milestone["dialogs"]),
				BuildChanges(milestone["changes"])
			);
			hash.insert(make_pair(milestone["milestone"].asString(), (int)milestones.size()));
			milestones.push_back(MilestoneNode(content));
		}
		for (int i = 0; i < milestones.size(); i++) {
			for (auto subsequence : root[i]["subsequences"]) {
                if (hash.find(subsequence.asString()) == hash.end())continue;
				if (hash[subsequence.asString()] < 0)continue;

				milestones[i].subsequents.push_back(&milestones[hash[subsequence.asString()]]);
				milestones[hash[subsequence.asString()]].premise++;
			}
		}
		for (auto &milestone : milestones) {
			if (milestone.premise == 0) {
				actives.push_back(&milestone);
			}
		}
	}
	else{
        THROW_EXCEPTION(JsonFormatException, "Json syntax error: " + reader.getFormattedErrorMessages() + ".\n");
	}
	fin.close();
}

void Script::ApplyChange(shared_ptr<Change> change) {
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

void Script::SaveStory(string path) {

}

void Script::LoadStory(string path) {

}

bool Script::JudgeCondition(Condition& condition) {
	return condition.EvaluateBool([this](string name) -> ValueType {
		return this->GetValue(name);
		});
}

pair<vector<Dialog>, vector<shared_ptr<Change>>> Script::MatchEvent(shared_ptr<Event> event) {
	pair<vector<Dialog>, vector<shared_ptr<Change>>> results;
	results.first.clear();
	results.second.clear();

	vector<MilestoneNode*> tmps;
    for (auto it = actives.begin(); it != actives.end(); ) {
		bool match = false;
		for (auto trigger : (*it)->content.GetTriggers()) {
			if (!JudgeCondition(trigger->GetCondition())) {
				continue;
			}

			if ((*it)->content.MatchTrigger(event)) {
				match = true;
				break;
			}
		}

		if (match) {
			auto subsequents = (*it)->subsequents;
			for (auto subsequent : subsequents) {
				subsequent->premise--;
				if (subsequent->premise <= 0) {
					tmps.push_back(subsequent);
				}
			}

			auto dialogs = (*it)->content.GetDialogs();
			results.first.insert(results.first.end(), dialogs.begin(), dialogs.end());
			auto changes = (*it)->content.GetChanges();
			results.second.insert(results.second.end(), changes.begin(), changes.end());

			if ((*it)->content.DropSelf([this](string name) -> ValueType {
				return this->GetValue(name);
				})) {
				it = actives.erase(it);
			}
			else {
				it++;
			}
		}
		else {
			it++;
		}
    }
	actives.insert(actives.end(), tmps.begin(), tmps.end());

	return results;
}

string Script::ReplaceContent(const string& content) {
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

vector<shared_ptr<Event>> Script::BuildEvent(Json::Value root) {
    vector<shared_ptr<Event>> events;

	for (auto obj : root) {
		shared_ptr<Event> event;

		string type = obj["type"].asString();
		if (type == "game_start") {
			event = make_shared<GameStartEvent>();
		}
		else if (type == "option_dialog") {
			event = make_shared<OptionDialogEvent>(obj["target"].asString(), obj["option"].asString());
		}
		else if (eventFactory->CheckRegistered(type)) {
			event = eventFactory->CreateEvent(type);
		}

		if (!event) {
			THROW_EXCEPTION(InvalidArgumentException, "Invalid event type: " + type + ".\n");
		}
		event->SetCondition(BuildCondition(obj["condition"]));
		events.push_back(event);
	}

    return events;
}

vector<Dialog> Script::BuildDialogs(Json::Value root) {
    vector<Dialog> dialogs;

    for (auto obj : root) {
        Dialog dialog;

        dialog.SetCondition(BuildCondition(obj["condition"]));

        for (auto speak : obj["list"]) {
            dialog.AddDialog(speak["speaker"].asString(), speak["content"].asString());
        }

        dialogs.push_back(dialog);
    }

    return dialogs;
}

vector<shared_ptr<Change>> Script::BuildChanges(Json::Value root) {
    vector<shared_ptr<Change>> changes;

    for (auto obj : root) {
        string type = obj["type"].asString();
        shared_ptr<Change> change = nullptr;

        if (type == "set_value") {
            change = make_shared<SetValueChange>(obj["variable"].asString(), obj["value"].asString());
        }
        else if (type == "remove_value") {
            change = make_shared<RemoveValueChange>(obj["variable"].asString());
        }
		else if (changeFactory->CheckRegistered(type)) {
			change = changeFactory->CreateChange(type);
		}

        if (!change) {
            THROW_EXCEPTION(InvalidArgumentException, "Invalid change type: " + type + ".\n");
        }

		change->SetCondition(BuildCondition(obj["condition"]));
        changes.push_back(change);
    }

    return changes;
}

Condition Script::BuildCondition(Json::Value root) {
	Condition condition;

	condition.ParseCondition(root.asString());

	return condition;
}

void Script::InitVariables() {
    variables["system.health_status"] = "healthy";
}

ValueType Script::GetValue(const std::string& name) {
	auto it = variables.find(name);
	if (it != variables.end()) {
		return it->second;
	}
	return 0; // 默认返回0
}

void Script::SetValue(const std::string& name, ValueType value) {
	variables[name] = value;
}


