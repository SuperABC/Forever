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

// 解析命令行输入事件
shared_ptr<Event> ParseEvent(Parser& parser) {
    string type = parser.GetOption("--type");
	int id;
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

int main() {
	// 读取Map相关类及Mod
	unique_ptr<Map> map(new Map());
	map->InitTerrains();
	map->InitRoadnets();
	map->InitZones();
	map->InitBuildings();
	map->InitComponents();
	map->InitRooms();
	map->ReadConfigs(REPLACE_PATH("../Resources/configs/config_map.json"));

	// 读取Populace相关类及Mod
	unique_ptr<Populace> populace(new Populace());
	populace->InitAssets();
	populace->InitJobs();
	populace->InitNames();
	populace->InitSchedulers();
	populace->ReadConfigs(REPLACE_PATH("../Resources/configs/config_populace.json"));

	// 读取Society相关类及Mod
	unique_ptr<Society> society(new Society());
	society->InitCalendars();
	society->InitOrganizations();
	society->ReadConfigs(REPLACE_PATH("../Resources/configs/config_society.json"));

	// 读取Story相关类及Mod
	unique_ptr<Story> story(new Story());
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
				populace->Init(map->Init(size, size));
				map->Checkin(populace->GetCitizens(), populace->GetTime());
				society->Init(map, populace);
				populace->Schedule();
				story->Init();
				string path = parser.GetOption("--story");
				story->ReadStory(path);
				populace->Characterize("../Resources/scripts/characters/", story);
				break;
			}
			case CMD_PASS: { // 时间流逝
				parser.AddOption("--tick", 0, "Ticks num to pass.", true, "0");
				parser.AddOption("--sec", 0, "Seconds num to pass.", true, "0");
				parser.AddOption("--min", 0, "Minutes num to pass.", true, "0");
				parser.AddOption("--day", 0, "Days num to pass.", true, "0");
				parser.ParseCmd(cmd);

				break;
			}
			case CMD_EVENT: { // 文本模拟事件
				parser.AddOption("--type", 0, "Event type.", true, "nothing_happen");

				parser.AddOption("--id", 0, "Event target id.", true, "");
				parser.AddOption("--target", 0, "Event target name.", true, "");
				parser.AddOption("--option", 0, "Event option.", true, "");

				parser.ParseCmd(cmd);
				auto event = ParseEvent(parser);

				if (true) {
					auto actions = story->MatchEvent(event);
					auto dialogs = actions.first;
					auto changes = actions.second;
					for (auto dialog : dialogs) {
						if (story->JudgeCondition(dialog.GetCondition())) {
							auto contents = dialog.GetDialogs();
							if (contents.size() > 0) {
								for (auto content : contents) {
									if (content.first.size() == 0)
										cout << story->ReplaceContent(content.second) << endl;
									else
										cout << story->ReplaceContent(content.first) << ": " << story->ReplaceContent(content.second) << endl;
								}
								break;
							}
						}
					}
					for (auto change : changes) {
						if (!story->JudgeCondition(change->GetCondition()))continue;
						map->ApplyChange(change, story);
						populace->ApplyChange(change, story);
						story->ApplyChange(change);
					}
				}
				if (event->GetType() == "option_dialog") {
					auto target = dynamic_pointer_cast<OptionDialogEvent>(event)->GetTarget();
					auto idx = dynamic_pointer_cast<OptionDialogEvent>(event)->GetIdx();
					pair<vector<Dialog>, vector<shared_ptr<Change>>> actions;
					if (target.size() == 0) {
						actions = populace->TriggerEvent(idx, event, story);
					}
					else {
						actions = populace->TriggerEvent(target, event, story);
					}
					auto dialogs = actions.first;
					auto changes = actions.second;
					for (auto dialog : dialogs) {
						if (story->JudgeCondition(dialog.GetCondition())) {
							auto contents = dialog.GetDialogs();
							if (contents.size() > 0) {
								for (auto content : contents) {
									if (content.first.size() == 0)
										cout << story->ReplaceContent(content.second) << endl;
									else
										cout << story->ReplaceContent(content.first) << ": " << story->ReplaceContent(content.second) << endl;
								}
								break;
							}
						}
					}
					for (auto change : changes) {
						if (!story->JudgeCondition(change->GetCondition()))continue;
						map->ApplyChange(change, story);
						populace->ApplyChange(change, story);
						story->ApplyChange(change);
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
				parser.ParseCmd(cmd);

				break;
			}

			// 输出帮助
			if (parser.HasOption("--help")) {
				parser.PrintHelp(type);
			}
		}
		catch (ExceptionBase &e) {
			cout << e.GetDetailedInfo() << endl;
		}

	} while (type != CMD_EXIT);

	return 0;
}