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
	// 读配置。文件打不开、JSON语法错、内容类型不对都不会抛异常：整份配置留空，原因记在GetLastReadError()。
	static void ReadConfig(const std::string& path);
	// 上一次ReadConfig读到一半出错的原因(单行文字)；成功、或只是文件打不开/语法错(那两种只打日志)时为空。
	static const std::string& GetLastReadError();

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

	// 顶层"seed"(Core各域随机流的种子来源)；没配置返回false。数字或字符串都接受。
	static bool GetGlobalSeed(unsigned long long& seed);

	// 顶层"map_size"(新开局的地图边长，单位=地图元素，1元素=10m)：写一个数=正方形，写[宽,高]=长方形；
	// 没配置/格式不对返回false(不兜底默认值，由调用方决定)。只做格式解析(正整数)，2的整数次幂等业务约束由调用方校验。
	static bool GetMapSize(int& width, int& height);

	// 主线脚本=Story screen的story_scripts列表(按顺序)。列表为空就是没有主线脚本，不兜底。
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
	// 单选concept(Roadnets/Names)当前启用的那个mod id；一个都没启用返回空串。配置里列了多个时取第一个并打警告
	// (界面和WriteConfig都会收口成一个，手改config.json才会出现)。
	static std::string GetUniqueConceptMod(const std::string& conceptKey);

	// 某个mod的参数字符串(整串，如"--path=x --skip")。已启用的读写conceptMods；未启用的读写内存暂存(不存盘)，
	// 之后启用该mod时带上暂存的参数，所以"禁用再启用"不丢参数。ReadConfig会清空暂存。
	static std::string GetModArgs(const std::string& conceptKey, const std::string& modId);
	static void SetModArgs(const std::string& conceptKey, const std::string& modId, const std::string& args);

	// 没列出的id视为未启用，新发现的mod默认禁用
	static bool IsModEnabled(const std::string& conceptKey, const std::string& modId);

	// 当前dll_paths下是否有dll提供这个(concept,id)——没提供的id在UI里不显示。
	static bool IsModDiscovered(const std::string& conceptKey, const std::string& modId);

	// 只认id字符串、不看concept，给依赖校验用
	static bool IsModIdEnabled(const std::string& id);

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
	static void ClearState();
	static void ReadConfigFile(const std::string& path);
	static std::string lastReadError;
	static bool CheckFileFormat(const std::filesystem::path& filePath, const std::string& format);


	// Roadnets/Names单选：数组里只保留最前面一个，见config.md
	static void NormalizeUniqueConcepts();
	// 丢掉未被发现的mod id和失效的main_story（WriteConfig保存前调用）。
	static void PruneUndiscoveredMods();

	static std::string configDir;

	static std::unordered_map<std::string, std::vector<std::string>> dllPaths;

	static std::unordered_map<std::string, std::vector<std::string>> layoutPaths;

	static std::unordered_map<std::string, std::vector<std::string>> resourcePaths;
	// 资源根目录的先后顺序(=config.json里resource_paths的顺序，之后新加的排在后面)。几个目录里有同名文件时
	// 排在前面的目录优先；写回config.json也按这个顺序。resourcePaths本身是无序表，不能拿它的遍历顺序当顺序用。
	static std::vector<std::string> resourceRootOrder;
	// 每个资源根目录下"文件名 -> 完整路径"的索引，AddResourcePath扫描时顺手建好(同一个目录里重名取先扫到的)。
	// GetResourceFilePath查它，不再每次递归扫盘。
	static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> resourceFilesByRoot;

	static std::unordered_map<std::string, std::vector<std::string>> pluginPaths;

	static std::unordered_map<std::string, std::vector<std::string>> pakPaths;

	static std::string mainStoryScriptModName;

	static bool hasGlobalSeed;
	static unsigned long long globalSeed;

	static bool hasMapSize;
	static int mapWidth;
	static int mapHeight;

	// "<concept>_mods" key -> (id, 参数字符串)列表，既是SetModArgs的参数来源，也是启用状态的唯一真相
	static std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> conceptMods;
	// 被禁用(或被单选挤掉)的mod的参数，仅内存、不写盘；key = jsonKey + '\n' + id。
	static std::unordered_map<std::string, std::string> stashedModArgs;
	static std::string StashKey(const std::string& jsonKey, const std::string& modId);

	static std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> dllConceptIds;

	// modId全局唯一，依赖按modId存
	static std::unordered_map<std::string, std::vector<std::string>> modDependences;

	static std::vector<std::string> storyScriptPaths;
};
