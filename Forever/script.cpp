#include "script.h"

#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <unordered_map>


using namespace std;

Script::Script() {

}

Script::Script(const shared_ptr<Script> script) {
	unordered_map<string, int> hash;
	hash["game_finish"] = -1;
	hash["game_fail"] = -2;

	for(auto& milestone : script->milestones) {
		Milestone content(
			milestone.content.GetName(),
			milestone.content.GetTriggers(),
			milestone.content.IsVisible(),
			milestone.content.DropCondition(),
			milestone.content.GetDescription(),
			milestone.content.GetGoal(),
			milestone.content.GetDialogs(),
			milestone.content.GetChanges()
		);
		hash.insert(make_pair(content.GetName(), (int)milestones.size()));
		milestones.push_back(MilestoneNode(content));
	}
	for (int i = 0; i < milestones.size(); i++) {
		for (auto subsequence : script->milestones[i].subsequents) {
			if (hash.find(subsequence->content.GetName()) == hash.end())continue;
			if (hash[subsequence->content.GetName()] < 0)continue;

			milestones[i].subsequents.push_back(&milestones[hash[subsequence->content.GetName()]]);
			milestones[hash[subsequence->content.GetName()]].premise++;
		}
	}
	for (auto& milestone : milestones) {
		if (milestone.premise == 0) {
			actives.push_back(&milestone);
		}
	}
}

Script::~Script() {

}

void Script::ReadScript(string path,
	unique_ptr<EventFactory> &eventFactory, unique_ptr<ChangeFactory> &changeFactory) {
	if (!filesystem::exists(path)) {
		THROW_EXCEPTION(IOException, "Path does not exist: " + path + ".\n");
	}

	JsonReader reader;
	JsonValue root;

	ifstream fin(path);
	if (!fin.is_open()) {
		THROW_EXCEPTION(IOException, "Failed to open file: " + path + ".\n");
	}

	//TODO: 多脚本合并
	if (reader.Parse(fin, root)) {
		unordered_map<string, int> hash;
		hash["game_finish"] = -1;
		hash["game_fail"] = -2;

		for (auto milestone : root) {
			Milestone content(
				milestone["milestone"].AsString(),
				BuildEvent(milestone["triggers"], eventFactory),
				milestone["visible"].AsBool(),
				BuildCondition(milestone["drop"].AsString()),
				milestone["description"].AsString(),
				milestone["goal"].AsString(),
				BuildDialogs(milestone["dialogs"], changeFactory),
				BuildChanges(milestone["changes"], changeFactory)
			);
			hash.insert(make_pair(milestone["milestone"].AsString(), (int)milestones.size()));
			milestones.push_back(MilestoneNode(content));
		}
		for (int i = 0; i < milestones.size(); i++) {
			for (auto subsequence : root[i]["subsequences"]) {
				if (hash.find(subsequence.AsString()) == hash.end())continue;
				if (hash[subsequence.AsString()] < 0)continue;

				milestones[i].subsequents.push_back(&milestones[hash[subsequence.AsString()]]);
				milestones[hash[subsequence.AsString()]].premise++;
			}
		}
		for (auto& milestone : milestones) {
			if (milestone.premise == 0) {
				actives.push_back(&milestone);
			}
		}
	}
	else {
		THROW_EXCEPTION(JsonFormatException, "Json syntax error: " + reader.GetErrorMessages() + ".\n");
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

			if ((*it)->content.DropSelf([&story](string name) -> pair<bool, ValueType> {
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

std::pair<std::vector<Dialog>, std::vector<std::shared_ptr<Change>>> Script::MatchEvent(
	std::shared_ptr<Event> event, Story* story, std::shared_ptr<Person> person) {
	pair<vector<Dialog>, vector<shared_ptr<Change>>> results;
	results.first.clear();
	results.second.clear();

	vector<MilestoneNode*> tmps;
	for (auto it = actives.begin(); it != actives.end(); ) {
		bool match = false;
		for (auto trigger : (*it)->content.GetTriggers()) {
			if (!story->JudgeCondition(trigger->GetCondition(), person)) {
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

			if ((*it)->content.DropSelf([&story](string name) -> pair<bool, ValueType> {
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

vector<shared_ptr<Event>> Script::BuildEvent(JsonValue root, unique_ptr<EventFactory> &factory) const {
	vector<shared_ptr<Event>> events;

	for (auto obj : root) {
		shared_ptr<Event> event;

		string type = obj["type"].AsString();
		if (type == "game_start") {
			event = make_shared<GameStartEvent>();
		}
		else if (type == "option_dialog") {
			event = make_shared<OptionDialogEvent>(obj["target"].AsString(), obj["option"].AsString());
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

vector<Dialog> Script::BuildDialogs(JsonValue root, std::unique_ptr<ChangeFactory>& factory) const {
	vector<Dialog> dialogs;

	for (auto obj : root) {
		Dialog dialog;

		dialog.SetCondition(BuildCondition(obj["condition"]));

		for (auto section : obj["list"]) {
			if (section.IsObject()) {
				dialog.AddDialog(section["speaker"].AsString(), section["content"].AsString());
			}
			else if (section.IsArray()) {
				vector<Option> options;
				for (auto item : section) {
					options.emplace_back(BuildCondition(item["condition"]), item["option"].AsString(),
						BuildDialogs(item["dialogs"], factory), BuildChanges(item["changes"], factory));
				}
				dialog.AddDialog(options);
			}
		}

		dialogs.push_back(dialog);
	}

	return dialogs;
}

vector<shared_ptr<Change>> Script::BuildChanges(JsonValue root, unique_ptr<ChangeFactory> &factory) const {
	vector<shared_ptr<Change>> changes;

	for (auto obj : root) {
		string type = obj["type"].AsString();
		shared_ptr<Change> change = nullptr;

		if (type == "set_value") {
			change = make_shared<SetValueChange>(obj["variable"].AsString(), obj["value"].AsString());
		}
		else if (type == "remove_value") {
			change = make_shared<RemoveValueChange>(obj["variable"].AsString());
		}
		else if (type == "spawn_npc") {
			change = make_shared<SpawnNpcChange>(obj["target"].AsString(), obj["gender"].AsString(), obj["birthday"].AsString());
		}
		else if (type == "add_option") {
			change = make_shared<AddOptionChange>(obj["target"].AsString(), obj["option"].AsString());
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

Condition Script::BuildCondition(JsonValue root) const {
	Condition condition;

	condition.ParseCondition(root.AsString());

	return condition;
}
