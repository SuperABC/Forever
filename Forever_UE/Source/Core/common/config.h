#pragma once

#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <filesystem>


// config.json的静态读写类，详见config.md
class Config {
public:
	// 找不到文件或JSON语法错误时静默留空，不抛异常
	static void ReadConfig(const std::string& path);

	static std::string GetConfigDir();

	static std::vector<std::string> GetDllPaths();

	static std::vector<std::string> GetMods();

	// 临时LoadLibrary探测后立即FreeLibrary，不持有句柄
	static void AddDllPath(const std::string& path);

	static void RemoveDllPath(const std::string& path);

	// 不去重：同名.layout后加载的覆盖先加载的
	static std::vector<std::string> GetLayouts();

	// 只收集路径，不探测/加载；脚本路径统一由resource_paths决定，见config.md
	static void AddResourcePath(const std::string& path);

	// config.json是否配置过resource_paths，调用方据此决定要不要回退默认目录
	static bool HasResourcePaths();

	// resource_paths扫描时顺带收集的.uplugin(编辑器挂载用)
	static std::vector<std::string> GetPlugins();

	// resource_paths扫描时顺带收集的.pak(打包挂载用)
	static std::vector<std::string> GetPakFiles();

	// 按bare文件名(不含扩展名)查.script绝对路径，找不到返回空串
	static std::string GetScriptPath(const std::string& name);

	// 按带扩展名的文件名在resource根目录下现扫递归查找(不缓存)，找不到返回空串
	static std::string GetResourceFilePath(const std::string& fileName);

	// main_story字段，缺失返回空串(不兜底默认值)
	static std::string GetMainStoryScriptModName();

	// 目前固定是bare名"test"，集中在这里避免字面量重复
	static std::string GetMainStoryScriptPath();

	// Story screen的story_scripts列表非空时用它，否则只有GetMainStoryScriptPath()
	static std::vector<std::string> GetMainStoryScriptPaths();

	// jsonKey形如"building_mods"，元素按第一个空格切成(id, 参数字符串)，参数原样返回不解析
	static std::vector<std::pair<std::string, std::string>> GetConceptMods(const std::string& jsonKey);

	// ---- 游戏启动配置界面(Story/Mod/Resource三screen)使用的接口 ----

	// 一行mod enable记录：某个dll提供的某一个(concept,modId)
	struct ModEntry {
		std::string dllPath;
		std::string conceptKey;
		std::string modId;
		bool enabled;
	};

	// 按(concept,modId)展开所有已发现mod；enabled即"id是否在对应<concept>_mods数组里"
	static std::vector<ModEntry> GetModEnables();

	// 粒度到id，直接增删<concept>_mods数组项(该数组是启用状态的唯一真相)
	// Roadnets/Names是"必须且只能启用一个"的单选concept：启用其一会取消同concept其它id，
	// 且对已启用的那个传enabled=false无效(不允许取消到一个都没有)。
	static void SetModEnabled(const std::string& conceptKey, const std::string& modId, bool enabled);

	// 是否是单选concept(Roadnets/Names)
	static bool IsUniqueConcept(const std::string& conceptKey);

	// 没列出的id视为未启用，新发现的mod默认禁用
	static bool IsModEnabled(const std::string& conceptKey, const std::string& modId);

	// 当前dll_paths下是否有dll提供这个(concept,id)——没提供的id在UI里不显示。
	static bool IsModDiscovered(const std::string& conceptKey, const std::string& modId);

	// 只认id字符串、不看concept，给依赖校验用
	static bool IsModIdEnabled(const std::string& id);

	// DLL里任意一个id启用即算"被加载"(DLL只能整体加载)，与依赖声明无关
	static bool IsDllActive(const std::string& dllPath);

	static std::vector<std::string> GetActiveDllPaths();

	// AddDllPath探测阶段缓存的(conceptKey,modId)列表
	static std::vector<std::pair<std::string, std::string>> GetModIdsForDll(const std::string& dllPath);

	// 这个modId声明依赖的mod id列表(per-modId而非per-DLL，见loader.h)
	static std::vector<std::string> GetModDependences(const std::string& modId);

	// resource根目录列表，供Resource screen用
	static std::vector<std::string> GetResourceRootPaths();

	// resourcePaths/pluginPaths/pakPaths/layoutPaths四个map里的key全部清掉
	static void RemoveResourcePath(const std::string& path);

	// Story screen的.script列表：全部一起加载，不是多选一
	static void AddStoryScript(const std::string& path);

	static void RemoveStoryScript(const std::string& path);

	static std::vector<std::string> GetStoryScripts();

	// 序列化写回config.json；只在Start Game校验通过时调用一次，不是每次编辑都保存
	static void WriteConfig(const std::string& path);

	// ---- 存档(见Source/Core/common/save.md) ----

	// conceptKey("Buildings")到jsonKey("building_mods")的转换，存档按jsonKey记mod
	static std::string ConceptKeyToJsonKey(const std::string& conceptKey);

	static std::vector<std::string> GetConceptJsonKeys();

	// 读档用：只改内存，不写回config.json
	static void SetConceptMods(const std::string& jsonKey, const std::vector<std::pair<std::string, std::string>>& mods);

	// 读档用：只改内存
	static void SetStoryScripts(const std::vector<std::string>& paths);
	static void SetMainStoryScriptModName(const std::string& name);

	// 存档资源检查：返回找不到的脚本名/layout模板名，全找到返回空
	static std::vector<std::string> FindMissingScripts(const std::vector<std::string>& scripts);
	static std::vector<std::string> FindMissingLayouts(const std::vector<std::string>& layouts);

	// 刷新缓存的文件列表(根目录添加时只扫一次)，开始新游戏/读档校验前调用
	static void RescanFiles();

private:
	static bool CheckFileFormat(const std::filesystem::path& filePath, const std::string& format);


	// Roadnets/Names单选：数组里只保留最前面一个，见config.md
	static void NormalizeUniqueConcepts();
	// 丢掉未被发现的mod id和失效的main_story（WriteConfig保存前调用）。
	static void PruneUndiscoveredMods();

	static std::string configDir;

	static std::unordered_map<std::string, std::vector<std::string>> dllPaths;

	static std::unordered_map<std::string, std::vector<std::string>> layoutPaths;

	static std::unordered_map<std::string, std::vector<std::string>> resourcePaths;

	static std::unordered_map<std::string, std::vector<std::string>> pluginPaths;

	static std::unordered_map<std::string, std::vector<std::string>> pakPaths;

	static std::string mainStoryScriptModName;

	// "<concept>_mods" key -> (id, 参数字符串)列表，既是SetModArgs的参数来源，也是启用状态的唯一真相
	static std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> conceptMods;

	static std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> dllConceptIds;

	// modId全局唯一，依赖按modId存
	static std::unordered_map<std::string, std::vector<std::string>> modDependences;

	static std::vector<std::string> storyScriptPaths;
};
