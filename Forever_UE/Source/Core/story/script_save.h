#pragma once

#include "common/save_archive.h"

#include <string>
#include <utility>
#include <vector>


class Map;
class Populace;
class Society;
class Traffic;
class Story;
class Player;
class Script;
struct SaveWorld;

// 全部Script的运行时状态的存档读写(段名SAVE_SECTION_SCRIPTS，定义在common/save.h)：每个Script
// 的里程碑premise、活动里程碑、变量、ScriptMod自己的状态。Script本身不在这里创建——各自的持有者
// (门/组织/岗位/Scheduler/车辆/Story)读档时已经按原来的方式建好了Script(读同样的.script文件)，
// 这里按持有者的稳定键找到它、把状态覆盖回去。详见script_save.md。
class ScriptSaveCodec {
public:
	static void Save(const SaveWorld& world, SaveArchive& archive);

	// 存档里没有这一段时返回false、error为空(调用方照新开局广播game_start)。
	static bool Load(const SaveArchive& archive, Map* map, Populace* populace, Society* society,
		Traffic* traffic, Story* story, std::string& error, Player* player = nullptr);

	// 持有者稳定键 -> Script：story.main/story.system、door:<门id>、org:<组织下标>、
	// job:<组织下标>:<岗位下标>、scheduler:<市民下标>、vehicle:<车名>、asset:<资产名>(玩家身上/地上/房间里带脚本的资产，含容器内容)。
	// 也供计时器用(Story的计时器按这个键记录归属，见story.h StoryTimer)。
	static std::vector<std::pair<std::string, Script*>> CollectScripts(Map* map, Populace* populace,
		Society* society, Traffic* traffic, Story* story, Player* player = nullptr);
};
