#include "parser.h"
#include "map.h"
#include "populace.h"

#include <iostream>
#include <memory>

#pragma comment(lib, "Forever.lib")
#pragma comment(lib, "Dependence.lib")


using namespace std;

int main() {
	// 读取Map相关类及Mod
	shared_ptr<Map> map = make_shared<Map>();
	shared_ptr<TerrainFactory> terrains = make_shared<TerrainFactory>();
	shared_ptr<RoadnetFactory> roadnets = make_shared<RoadnetFactory>();
	shared_ptr<ZoneFactory> zones = make_shared<ZoneFactory>();
	shared_ptr<BuildingFactory> buildings = make_shared<BuildingFactory>();
	shared_ptr<ComponentFactory> components = make_shared<ComponentFactory>();
	shared_ptr<RoomFactory> rooms = make_shared<RoomFactory>();
	map->InitTerrains(terrains);
	map->InitRoadnets(roadnets);
	map->InitZones(zones);
	map->InitBuildings(buildings);
	map->InitComponents(components);
	map->InitRooms(rooms);

	// 读取Populace相关类及Mod
	shared_ptr<Populace> populace = make_shared<Populace>();
	shared_ptr<JobFactory> jobs = make_shared<JobFactory>();
	populace->InitJobs(jobs);

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
				parser.AddOption("--story", 0, "Story file.", true, "script/ys.json");
				parser.ParseCmd(cmd);

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

				break;
			}
			case CMD_PRINT: { // 输出当前状态
				parser.AddOption("--map", 'm', "Whether to print the map.", false);
				parser.AddOption("--populace", 'p', "Whether to print the populace.", false);
				parser.AddOption("--script", 's', "Whether to print the script.", false);

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
		catch (exception e) {
			cout << e.what() << endl;
		}

	} while (type != CMD_EXIT);

	return 0;
}