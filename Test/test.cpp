#include "parser.h"
#include "map.h"
#include "populace.h"
#include "society.h"
#include "story.h"
#include "utility.h"
#include "error.h"

#include <iostream>
#include <memory>

#pragma comment(lib, "Forever.lib")
#pragma comment(lib, "Dependence.lib")


using namespace std;

unique_ptr<Map> map = make_unique<Map>();
unique_ptr<Populace> populace = make_unique<Populace>();
unique_ptr<Society> society = make_unique<Society>();
unique_ptr<Story> story = make_unique<Story>();

// 解析命令行输入事件
shared_ptr<Event> ParseEvent(Parser& parser) {
	string type = parser.GetOption("--type");
	int id = -1;
	string target = "";
	string option = "";

	if (type == "game_start") {
		return make_shared<GameStartEvent>();
	}
	else if (type == "option_dialog") {
		if (parser.HasOption("--id")) id = stoi(parser.GetOption("--id"));
		if (parser.HasOption("--target")) target = parser.GetOption("--target");
		if (parser.HasOption("--option")) option = parser.GetOption("--option");
		if (target.size() == 0)
			return make_shared<OptionDialogEvent>(id, option);
		else {
			return make_shared<OptionDialogEvent>(target, option);
		}
	}

	return nullptr;
}

bool PrintDialog(Dialog dialog, std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues) {
	auto sections = dialog.GetDialogs();
	if (sections.size() == 0)return false;

	for (auto section : sections) {
		if (section.IsBranch()) {
			auto options = section.GetOptions();
			vector<int> indices;
			int i = 0, j = 0;
			for (auto option : options) {
				if (story->JudgeCondition(option.GetCondition(), getValues)) {
					indices.push_back(i++);
				}
			}
			cout << "Options: " << endl;
			for (auto index : indices) {
				cout << j << ": " << options[indices[j++]].GetOption() << endl;
			}
			cin >> j;
			auto selected = options[indices[j]];
			for (auto dialog : selected.GetDialogs()) {
				PrintDialog(dialog, getValues);
			}
			for (auto change : selected.GetChanges()) {
				if (!story->JudgeCondition(change->GetCondition()))continue;
				::map->ApplyChange(change, story);
				populace->ApplyChange(change, story);
				story->ApplyChange(change);
			}
		}
		else {
			Condition conditionContent;
			conditionContent.ParseCondition(section.GetSpeaking().second);
			if (section.GetSpeaking().first.size() == 0) {
				cout << ToString(conditionContent.EvaluateValue(getValues)) << endl;
			}
			else {
				Condition conditionSpeaker;
				conditionSpeaker.ParseCondition(section.GetSpeaking().first);
				cout << ToString(conditionSpeaker.EvaluateValue(getValues)) << ": " <<
					ToString(conditionContent.EvaluateValue(getValues)) << endl;
			}
		}
	}
	return true;
}

int main() {
	// 读取Map相关类及Mod
	::map->ReadConfigs(REPLACE_PATH("../Resources/configs/config_map.json"));
	::map->InitTerrains();
	::map->InitRoadnets();
	::map->InitZones();
	::map->InitBuildings();
	::map->InitComponents();
	::map->InitRooms();

	// 读取Populace相关类及Mod
	populace->ReadConfigs(REPLACE_PATH("../Resources/configs/config_populace.json"));
	populace->InitAssets();
	populace->InitJobs();
	populace->InitNames();
	populace->InitSchedulers();

	// 读取Society相关类及Mod
	society->ReadConfigs(REPLACE_PATH("../Resources/configs/config_society.json"));
	society->InitCalendars();
	society->InitOrganizations();

	// 读取Story相关类及Mod
	story->ReadConfigs(REPLACE_PATH("../Resources/configs/config_story.json"));
	story->InitEvents();
	story->InitChanges();

	// 读取命令行
	string cmd;
	CMD_TYPE type = CMD_UNKOWN;
	do {
		// 读取一行命令
		string input;
		getline(cin, input);

		// 添加帮助选项
		Parser parser;
		parser.AddOption("--help", 'h', "Show help.");

		// 空格分解命令
		auto cmd = parser.ExtractCmd(input);
		if (cmd.size() <= 0)continue;
		auto type = parser.GetAction(cmd[0]);

		try {
			switch (type) {
			case CMD_INIT: { // 初始化世界、人口、剧本
				parser.AddOption("--block", 0, "Block num both horizontally and vertically.", true, "4");
				parser.AddOption("--story", 0, "Story file.", true, REPLACE_PATH("../Resources/scripts/ys.json"));
				parser.ParseCmd(cmd);
				int size = atoi(parser.GetOption("--block").data());
				populace->Init(::map->Init(size, size));
				::map->Checkin(populace->GetCitizens(), populace->GetTime());
				society->Init(::map, populace);
				populace->Schedule();
				story->Init(populace);
				string path = parser.GetOption("--story");
				story->ReadStory(path);
				populace->Jobstory(story);
				populace->Characterize("../Resources/scripts/characters/", story);
				break;
			}
			case CMD_PASS: { // 时间流逝
				parser.AddOption("--sec", 0, "Seconds num to pass.", true, "0");
				parser.AddOption("--min", 0, "Minutes num to pass.", true, "0");
				parser.AddOption("--hour", 0, "Hours num to pass.", true, "0");
				parser.AddOption("--day", 0, "Days num to pass.", true, "0");
				parser.ParseCmd(cmd);

				int days = 0, hours = 0, mins = 0, secs = 0;
				if (parser.HasOption("--sec")) {
					secs = atoi(parser.GetOption("--sec").data());
				}
				if (parser.HasOption("--min")) {
					mins = atoi(parser.GetOption("--min").data());
				}
				if (parser.HasOption("--hour")) {
					hours = atoi(parser.GetOption("--hour").data());
				}
				if (parser.HasOption("--day")) {
					days = atoi(parser.GetOption("--day").data());
				}

				::map->Tick(days, hours, mins, secs);
				populace->Tick(::map, days, hours, mins, secs);
				society->Tick(days, hours, mins, secs);
				story->Tick(days, hours, mins, secs);

				break;
			}
			case CMD_EVENT: { // 文本模拟事件
				parser.AddOption("--type", 0, "Event type.", true, "nothing_happen");

				parser.AddOption("--id", 0, "Event target id.", true, "");
				parser.AddOption("--target", 0, "Event target name.", true, "");
				parser.AddOption("--option", 0, "Event option.", true, "");

				parser.ParseCmd(cmd);
				auto event = ParseEvent(parser);
				if (!event) {
					THROW_EXCEPTION(CommandException, "Wrong input format.");
				}

				if (true) {
					std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues = {
						[&](string name) -> pair<bool, ValueType> {
							return story->GetValue(name);
						}
					};

					auto actions = story->MatchEvent(event);
					auto dialogs = actions.first;
					auto changes = actions.second;
					for (auto dialog : dialogs) {
						if (story->JudgeCondition(dialog.GetCondition())) {
							if(PrintDialog(dialog, getValues)) {
								break;
							}
						}
					}
					for (auto change : changes) {
						if (!story->JudgeCondition(change->GetCondition()))continue;
						::map->ApplyChange(change, story);
						populace->ApplyChange(change, story);
						story->ApplyChange(change);
					}
				}
				if (event->GetType() == "option_dialog") {
					auto target = dynamic_pointer_cast<OptionDialogEvent>(event)->GetTarget();
					auto idx = dynamic_pointer_cast<OptionDialogEvent>(event)->GetIdx();
					pair<vector<Dialog>, vector<shared_ptr<Change>>> actions;
					shared_ptr<Person> person = nullptr;
					if (target.size() == 0) {
						actions = populace->TriggerEvent(idx, event, story);
						person = populace->GetCitizens()[idx];
					}
					else {
						actions = populace->TriggerEvent(target, event, story);
						person = populace->GetCitizen(target);
					}

					if (person == nullptr) {
						THROW_EXCEPTION(CommandException, "Citizen not found.");
					}

					std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues = {
						[&](string name) -> pair<bool, ValueType> {
							return story->GetValue(name);
						},
						[&](string name) -> pair<bool, ValueType> {
							return person->GetValue(name);
						}
					};

					auto dialogs = actions.first;
					auto changes = actions.second;
					for (auto dialog : dialogs) {
						if (story->JudgeCondition(dialog.GetCondition())) {
							if (PrintDialog(dialog, getValues)) {
								break;
							}
						}
					}
					for (auto change : changes) {
						if (!story->JudgeCondition(change->GetCondition()))continue;
						::map->ApplyChange(change, story);
						populace->ApplyChange(change, story);
						story->ApplyChange(change);
					}
				}
				break;
			}
			case CMD_LOOKUP: { // 查找信息
				parser.AddOption("--id", 0, "Lookup citizen by id.", true, "");
				parser.AddOption("--name", 0, "Lookup citizen by name.", true, "");
				parser.ParseCmd(cmd);

				if (parser.HasOption("--id")) {
					int id = atoi(parser.GetOption("--id").data());
					auto citizen = populace->GetCitizens()[id];

					cout << "Citizen ID: " << citizen->GetId() << endl;
					cout << "Name: " << citizen->GetName() << endl;
					cout << "Age: " << citizen->GetAge(populace->GetTime()) << endl;
					cout << "Options: " << endl;
					for (auto option : citizen->GetOptions()) {
						cout << "--" << option << endl;
					}
				}
				else if (parser.HasOption("--name")) {
					string name = parser.GetOption("--name");
					for (auto citizen : populace->GetCitizens()) {
						if (citizen->GetName() == name) {
							cout << "Citizen ID: " << citizen->GetId() << endl;
							cout << "Name: " << citizen->GetName() << endl;
							cout << "Age: " << populace->GetTime().GetYear() - citizen->GetBirthday().GetYear() << endl;
							cout << "Options: " << endl;
							for (auto option : citizen->GetOptions()) {
								cout << "--" << option << endl;
							}
							break;
						}
					}
				}
				break;
			}
			case CMD_PRINT: { // 输出当前状态
				parser.AddOption("--map", 'm', "Whether to print the map.", false);
				parser.AddOption("--populace", 'p', "Whether to print the populace.", false);
				parser.AddOption("--story", 's', "Whether to print the story.", false);

				parser.ParseCmd(cmd);
				break;
			}
			default:
				THROW_EXCEPTION(CommandException, "Wrong input format.");
				break;
			}

			// 输出帮助
			if (parser.HasOption("--help")) {
				parser.PrintHelp(type);
			}
		}
		catch (ExceptionBase& e) {
			cout << e.GetDetailedInfo() << endl;
		}

	} while (type != CMD_EXIT);

	return 0;
}