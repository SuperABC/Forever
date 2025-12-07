#include "script.h"

#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <unordered_map>


using namespace std;

Script::Script() {

}

Script::~Script() {

}

void Script::ReadScript(string path,
	std::unique_ptr<EventFactory> &eventFactory, std::unique_ptr<ChangeFactory> &changeFactory) {
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
				BuildEvent(milestone["triggers"], eventFactory),
				milestone["visible"].asBool(),
				BuildCondition(milestone["drop"].asString()),
				milestone["description"].asString(),
				milestone["goal"].asString(),
				BuildDialogs(milestone["dialogs"]),
				BuildChanges(milestone["changes"], changeFactory)
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
		for (auto& milestone : milestones) {
			if (milestone.premise == 0) {
				actives.push_back(&milestone);
			}
		}
	}
	else {
		THROW_EXCEPTION(JsonFormatException, "Json syntax error: " + reader.getFormattedErrorMessages() + ".\n");
	}
	fin.close();
}

pair<vector<Dialog>, vector<shared_ptr<Change>>> Script::MatchEvent(
	shared_ptr<Event> event, Story *story) {
	pair<vector<Dialog>, vector<shared_ptr<Change>>> results;
	results.first.clear();
	results.second.clear();

	vector<MilestoneNode*> tmps;
	for (auto it = actives.begin(); it != actives.end(); ) {
		bool match = false;
		for (auto trigger : (*it)->content.GetTriggers()) {
			if (!story->JudgeCondition(trigger->GetCondition())) {
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

			if ((*it)->content.DropSelf([&story](string name) -> ValueType {
				return story->GetValue(name);
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

vector<shared_ptr<Event>> Script::BuildEvent(Json::Value root, unique_ptr<EventFactory> &factory) {
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
		else if (factory->CheckRegistered(type)) {
			event = factory->CreateEvent(type);
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

vector<shared_ptr<Change>> Script::BuildChanges(Json::Value root, unique_ptr<ChangeFactory> &factory) {
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
		else if (factory->CheckRegistered(type)) {
			change = factory->CreateChange(type);
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
