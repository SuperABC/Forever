#pragma once

#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <filesystem>


// 读取/维护config.json的静态类:dll_paths/resource_paths两类根目录扫描、
// 每个concept的mod参数与启用状态、主线剧情脚本路径,并能把当前内存状态写回文件
// (WriteConfig)。详见同目录config.md。
class Config {
public:
	// 读取path指向的config.json,清空并重建dllPaths/resourcePaths/conceptMods。内部对
	// dll_paths/resource_paths两个数组逐项分别调用AddDllPath/AddResourcePath(相对路径都相对
	// config.json自己所在目录configDir解析);并把所有以"_mods"结尾的顶层key当作一个concept
	// 的mod列表解析(不需要硬编码21个concept的名字,配置文件本身决定内容)。找不到文件或json
	// 语法错误时静默留空,不抛异常,调用方据此决定要不要回退到硬编码默认目录。
	static void ReadConfig(const std::string& path);

	// 当前生效的config.json所在目录。
	static std::string GetConfigDir();

	// 已注册的mod根目录列表(即调用过AddDllPath的path)。
	static std::vector<std::string> GetDllPaths();

	// 所有已发现、探测通过的dll绝对路径(跨目录去重后扁平化)。
	static std::vector<std::string> GetMods();

	// 递归扫描path下所有.dll,临时LoadLibraryA+探测是否支持任意一个已知concept的
	// GetMod<Concept>符号,探测完立即FreeLibrary——不长期持有句柄,持有句柄是
	// ModLoader注册阶段的职责。
	static void AddDllPath(const std::string& path);

	// 移除path对应的已注册dll路径记录。
	static void RemoveDllPath(const std::string& path);

	// 所有已发现的.layout绝对路径(跨目录扁平化，不去重——理论上不会有同名冲突，
	// BuildingLayoutLibrary按文件basename为key，后加载的会覆盖先加载的，调用方自己保证
	// 不重复摆放同名模板)。
	static std::vector<std::string> GetLayouts();

	// 递归扫描path下所有.script文件，记下绝对路径——和AddDllPath同一个手法，不需要
	// 探测/加载任何东西，找到就收，不做格式校验(交给Script::ReadScript解析时再报错)。
	// JobMod/OrganizationMod（以及Story）只写一个不含路径/扩展名的bare文件名（如
	// "job_shop_saler"），不知道也不需要知道用户实际把脚本文件放在哪，由这里的
	// resource_paths配置统一决定实际存放位置，见job.md"按需查地址"一节的"Script配置"说明。
	static void AddResourcePath(const std::string& path);

	// 是否已经有任何resource_paths被注册过——config.json没有配置resource_paths(或者根本
	// 没有config.json)时，调用方(ForeverModSubsystem)据此决定要不要回退扫描默认的
	// Resource/Story目录，和dll_paths同一个回退容错风格。
	static bool HasResourcePaths();

	// 所有已发现的.uplugin绝对路径——和GetLayouts()同一个手法，resource_paths扫描时按
	// 后缀顺带收集，不需要单独的plugin_paths数组。供ForeverModSubsystem在WITH_EDITOR下
	// 直接RegisterMountPoint到这个plugin的Content目录(不需要cook/pak)，见config.md。
	static std::vector<std::string> GetPlugins();

	// 所有已发现的.pak绝对路径——和GetPlugins()同一个来源，供ForeverModSubsystem在非编辑器
	// (打包)下MountPaksEx挂载，见config.md。
	static std::vector<std::string> GetPakFiles();

	// 按不含路径/扩展名的bare文件名查找对应.script文件的绝对路径——遍历所有已发现的
	// .script路径，返回第一个basename(不含扩展名)等于name的；找不到返回空字符串。不做
	// 任何缓存/去重校验，理论上不会有同名冲突，调用方自己保证不重复摆放同名脚本。
	static std::string GetScriptPath(const std::string& name);

	// main_story字段：主线剧情Script要挂载的ScriptModName，不再由Story硬编码"empty"。
	// 字段缺失时返回空字符串，调用方(Story::Init)据此判定跳过初始化，不在这里兜底默认值。
	static std::string GetMainStoryScriptModName();

	// 主线剧情.script文件的实际路径——目前固定是bare名字"test"（Story::Init()一直这么
	// 写），包一层是因为name_reserve/global_settings/mod_dependences校验需要在Story
	// 对象创建之前就拿到同一份路径，避免"test"这个字面量到处重复。等以后支持多主线剧情时
	// 再改这个方法内部的实现，调用方不用跟着改。
	static std::string GetMainStoryScriptPath();

	// 主线剧情要读取的全部.script文件：Story screen里玩家添加的story_scripts列表非空时就是这份
	// 列表(按顺序，每个文件的milestones合并进同一个主线Script，names/name_reserve、
	// global_settings也合并)；列表为空时退回只有GetMainStoryScriptPath()这一个文件。
	static std::vector<std::string> GetMainStoryScriptPaths();

	// jsonKey形如"building_mods"。返回该数组解析出的(id, 参数字符串)列表——每个数组元素
	// 按第一个空格切成两段,如"pengzhan --density 1.0"切成("pengzhan", "--density 1.0"),
	// 没有空格则参数为空串。参数字符串原样返回,格式/是否使用完全由mod自己的creator(收到
	// 这个字符串后决定怎么用)决定,Config不做任何解析。jsonKey不存在时返回空列表。
	static std::vector<std::pair<std::string, std::string>> GetConceptMods(const std::string& jsonKey);

	// ---- 游戏启动配置界面(Story/Mod/Resource三screen)新增 ----

	// 一行mod enable记录：某个dll提供的某一个(concept,modId)，供UI按行展示。
	struct ModEntry {
		std::string dllPath;
		std::string conceptKey;
		std::string modId;
		bool enabled;
	};

	// 按(concept,modId)粒度展开当前所有已发现mod的启用状态，一个dll可能贡献多行(它导出的
	// 每个concept每个id各一行)。enabled直接等于"这个id当前在不在对应的`<concept>_mods`
	// 数组里"——不是独立维护的一份状态，见IsModEnabled。
	static std::vector<ModEntry> GetModEnables();

	// 设置某个(concept,modId)的启用/禁用——粒度到id,不是到dll(同一个dll导出的多个id可以
	// 分别独立启用/禁用)。实现上就是把这个id加进/从对应的`<concept>_mods`数组里删掉(保留/
	// 丢弃它原有的参数字符串)，这张数组本来就是这个工程"谁启用谁没启用"的唯一真相来源
	// (Registry::ReloadModArgs→Factory::SetModArgs/IsEnabled直接读它)，不需要再维护一份
	// 平行的勾选状态。
	static void SetModEnabled(const std::string& conceptKey, const std::string& modId, bool enabled);

	// 某个(concept,modId)当前是否启用——就是"这个id在不在`<concept>_mods`数组里"，缺省
	// (数组里没有/jsonKey整个没出现过)视为false,和这个工程一直以来"没列出的id视为未启用"
	// 的约定一致。新发现的mod(刚AddDllPath扫出来的)默认是禁用的,需要在Mod screen里手动
	// 勾选一次才会真正生效。
	static bool IsModEnabled(const std::string& conceptKey, const std::string& modId);

	// 这个mod id(不带concept前缀,和Registry::CheckModRegistered同样的"只认id字符串,不看
	// concept"匹配规则)是否存在至少一个已启用的(concept,modId==id)——给依赖校验用。
	static bool IsModIdEnabled(const std::string& id);

	// dllPath这个DLL是否"被加载"——它导出的(concept,modId)里只要有任意一个启用就算数,DLL
	// 本身只能整体加载/不加载。注意这和依赖声明无关——依赖声明现在是per-modId的，见
	// GetModDependences注释；这个函数只给ForeverModSubsystem判断"要不要把这个DLL交给
	// ModLoader"用。
	static bool IsDllActive(const std::string& dllPath);

	// 当前"被加载"(IsDllActive==true)的dll绝对路径列表。
	static std::vector<std::string> GetActiveDllPaths();

	// dllPath这个DLL探测时读到的(conceptKey,modId)列表——AddDllPath探测阶段顺带读取、
	// 缓存下来，不需要重新LoadLibrary。
	static std::vector<std::pair<std::string, std::string>> GetModIdsForDll(const std::string& dllPath);

	// modId这个mod id自己声明依赖的mod id列表(来自它所在DLL导出的GetModDependences()配对
	// 表里属于这个modId的那一项)——AddDllPath探测阶段顺带读取、按modId拆开缓存下来，不是
	// 按DLL分组，见loader.h kModDependenciesSymbol注释。
	static std::vector<std::string> GetModDependences(const std::string& modId);

	// 已注册的resource根目录列表(即调用过AddResourcePath的path)，供Resource screen展示/
	// 删除用，和GetDllPaths()同样的"根目录列表"语义。
	static std::vector<std::string> GetResourceRootPaths();

	// 移除path对应的已注册resource根目录记录(resourcePaths/pluginPaths/pakPaths/
	// layoutPaths四个map里这个key全部清掉——layoutPaths也要清是因为AddResourcePath同一次
	// 扫描顺带收集.layout，见下面AddResourcePath的注释)。
	static void RemoveResourcePath(const std::string& path);

	// 校验path是.script文件且存在后加入storyScriptPaths(按绝对路径去重)——Story screen
	// 的"添加.script文件"操作，这个列表里的脚本全部一起加载，不是"多选一激活"。
	static void AddStoryScript(const std::string& path);

	// 从storyScriptPaths移除path(按绝对路径匹配)。
	static void RemoveStoryScript(const std::string& path);

	// 当前Story screen列表里的全部.script绝对路径。
	static std::vector<std::string> GetStoryScripts();

	// 把当前内存状态(dllPaths/layoutPaths/resourcePaths的根目录、main_story、
	// storyScriptPaths、conceptMods)序列化写回path指向的config.json——只在Start Game
	// 校验通过的那一刻调用一次,不是每次编辑都自动保存。路径按ReadConfig同样的"相对
	// configDir"约定写成相对路径。"<concept>_mods"这几个数组就是conceptMods原样写回——
	// SetModEnabled已经直接维护这份状态了，不需要在这里另外重建。
	static void WriteConfig(const std::string& path);

	// ---- 存档(见Source/Core/common/save.md) ----

	// conceptKey(如"Buildings")到config.json里"<concept>_mods"数组key(如"building_mods")的转换，
	// 存档按jsonKey记mod，和GetModEnables()返回的conceptKey对得上就靠它。
	static std::string ConceptKeyToJsonKey(const std::string& conceptKey);

	// 当前内存里出现过的全部"<concept>_mods" jsonKey。
	static std::vector<std::string> GetConceptJsonKeys();

	// 整个替换某个concept的(id, 参数字符串)列表——读档时把存档用到的mod集合设成当前启用集合，
	// 只改内存，不写回config.json。
	static void SetConceptMods(const std::string& jsonKey, const std::vector<std::pair<std::string, std::string>>& mods);

	// 整个替换Story screen的脚本列表(绝对路径)，读档时按存档里的脚本名重新解析出来的路径。
	static void SetStoryScripts(const std::vector<std::string>& paths);
	static void SetMainStoryScriptModName(const std::string& name);

	// 存档的资源检查：scripts是不含路径/扩展名的脚本名(按GetScriptPath查)，layouts是.layout
	// 模板名(按GetLayouts()的文件名去扩展名查)。返回找不到的条目(脚本名/模板名原样)，全找到返回空。
	static std::vector<std::string> FindMissingScripts(const std::vector<std::string>& scripts);
	static std::vector<std::string> FindMissingLayouts(const std::vector<std::string>& layouts);

	// 重新扫描所有已配置的layout/resource根目录，刷新缓存的.layout/.script/.uplugin/.pak文件列表。
	// 文件列表只在添加根目录时扫一次，游戏运行中往目录里增删文件不会自动反映——开始新游戏和
	// 读档校验前调用一次，保证用的是磁盘上的当前内容(根目录本身不增不减)。
	static void RescanFiles();

private:
	static bool CheckFileFormat(const std::filesystem::path& filePath, const std::string& format);


	// Roadnets/Names这两个concept一次只应该有一个生效(RoadnetFactory::GetRoadnet()/
	// NameFactory::GetName()的"单选"语义，见对应_factory.h)——但那两个函数本身并不校验
	// "只有一个enabled"，`SetConfig`调几次都行，`GetRoadnet`/`GetName`只是从
	// `unordered_map`里捞出第一个碰到的`enabled==true`，其余被标记启用但没被选中的id
	// 直接静默忽略，不报错。游戏启动配置界面要求"看起来"也必须唯一(不只是生效结果唯一)，
	// 所以在`Config`层面显式规范化：对应的`<concept>_mods`数组里如果同时有多个条目，只保留
	// 数组里排在最前面的那一个，其余直接从数组删掉——在`ReadConfig`读完文件后、
	// `WriteConfig`写文件前、`GetModEnables()`每次给UI返回数据前都调用一次，保证UI勾选框
	// 显示的状态、保存到文件的状态永远和这条约束一致。
	static void NormalizeUniqueConcepts();

	static std::string configDir;

	// mod根目录path -> 该目录下发现的、探测通过的dll绝对路径列表
	static std::unordered_map<std::string, std::vector<std::string>> dllPaths;

	// layout根目录path -> 该目录下发现的.layout绝对路径列表
	static std::unordered_map<std::string, std::vector<std::string>> layoutPaths;

	// resource根目录path -> 该目录下发现的.script绝对路径列表，结构和layoutPaths一致
	static std::unordered_map<std::string, std::vector<std::string>> resourcePaths;

	// resource根目录path -> 该目录下发现的.uplugin绝对路径列表，AddResourcePath同一次扫描
	// 顺带收集，结构和resourcePaths一致。
	static std::unordered_map<std::string, std::vector<std::string>> pluginPaths;

	// resource根目录path -> 该目录下发现的.pak绝对路径列表，同上。
	static std::unordered_map<std::string, std::vector<std::string>> pakPaths;

	// main_story字段的原始值(ScriptModName)，ReadConfig时读取
	static std::string mainStoryScriptModName;

	// "<concept>_mods"这个json key -> 该数组解析出的(id, 参数字符串)列表——这既是
	// Factory::SetModArgs的参数来源，也是"谁启用谁没启用"的唯一真相来源(IsModEnabled/
	// SetModEnabled直接读写这张表，不再有平行的一份启用状态)。
	static std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> conceptMods;

	// dll绝对路径 -> AddDllPath探测时读到的(conceptKey,modId)列表。
	static std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> dllConceptIds;

	// modId -> 它自己声明依赖的mod id列表(来自所在DLL的GetModDependences()配对表)，按
	// modId拆开存，不是按dllPath分组——modId本身全局唯一，不需要再绕一层DLL路径。
	static std::unordered_map<std::string, std::vector<std::string>> modDependences;

	// Story screen里玩家显式添加的.script绝对路径列表，全部一起加载。
	static std::vector<std::string> storyScriptPaths;
};
