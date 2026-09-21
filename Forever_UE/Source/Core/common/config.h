#pragma once

#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <filesystem>


// 阶段3裁剪版Config:只保留"读取config.json + 扫描dll_paths/layout_paths目录 + 每个concept
// 的mod参数列表"相关的接口(第N轮迁移——Building内部布局落地时补回了layout_paths/
// AddLayoutPath/GetLayouts，不再是纯占位)。旧工程Config类里的启用禁用状态
// (GetChecks/CheckMod/GetEnables)、老式"一个资源目录桶四种后缀分流"的资源目录扫描
// (GetResourcePaths/GetScripts/GetPlugins/GetPakFiles/AddResourcePath/RemoveResourcePath)、
// 全局设置(GetGlobalSettings)、主剧情路径(GetStories/AddScript/RemoveScript)、运行时
// 写回(WriteConfig)仍未迁移,等对应机制/系统在阶段4落地时按需加回,详见同目录config.md。
class Config {
public:
	// 读取path指向的config.json,清空并重建dllPaths/layoutPaths/conceptMods。内部对
	// dll_paths/layout_paths两个数组逐项分别调用AddDllPath/AddLayoutPath(相对路径都相对
	// config.json自己所在目录configDir解析);并把所有以"_mods"结尾的顶层key当作一个concept
	// 的mod列表解析(不需要硬编码20个concept的名字,配置文件本身决定内容)。找不到文件或json
	// 语法错误时静默留空,不抛异常(阶段3约定,调用方据此决定要不要回退到硬编码默认目录)。
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

	// 递归扫描path下所有.layout文件，记下绝对路径——和AddDllPath同一个
	// std::filesystem::recursive_directory_iterator手法，但不需要探测/加载任何东西
	// (.layout是纯文本模板文件，不是dll)，找到就收，不做格式校验(交给
	// BuildingLayoutLibrary::ReadTemplates解析时再报错)。
	static void AddLayoutPath(const std::string& path);

	// 所有已发现的.layout绝对路径(跨目录扁平化，不去重——理论上不会有同名冲突，
	// BuildingLayoutLibrary按文件basename为key，后加载的会覆盖先加载的，调用方自己保证
	// 不重复摆放同名模板)。
	static std::vector<std::string> GetLayouts();

	// 递归扫描path下所有.script文件，记下绝对路径——和AddLayoutPath同一个手法，不需要
	// 探测/加载任何东西，找到就收，不做格式校验(交给Script::ReadScript解析时再报错)。
	// 这次新增的动机：JobMod/OrganizationMod（以及Story）只应该写一个不含路径/扩展名的
	// bare文件名（如"job_shop_saler"），不应该也不可能知道用户实际把脚本文件放在哪，
	// 由这里的resource_paths配置统一决定实际存放位置，见job.md"按需查地址"一节旁边新增的
	// "Script配置"说明。
	static void AddResourcePath(const std::string& path);

	// 是否已经有任何resource_paths被注册过——config.json没有配置resource_paths(或者根本
	// 没有config.json)时，调用方(ForeverModSubsystem)据此决定要不要回退扫描默认的
	// Resource/Story目录，和dll_paths/layout_paths同一个回退容错风格。
	static bool HasResourcePaths();

	// 按不含路径/扩展名的bare文件名查找对应.script文件的绝对路径——遍历所有已发现的
	// .script路径，返回第一个basename(不含扩展名)等于name的；找不到返回空字符串。不做
	// 任何缓存/去重校验，理论上不会有同名冲突，调用方自己保证不重复摆放同名脚本。
	static std::string GetScriptPath(const std::string& name);

	// main_story字段：主线剧情Script要挂载的ScriptModName，不再由Story硬编码"empty"。
	// 字段缺失时返回空字符串，调用方(Story::Init)据此判定跳过初始化，不在这里兜底默认值。
	static std::string GetMainStoryScriptModName();

	// 主线剧情.script文件的实际路径——目前固定是bare名字"test"（Story::Init()一直这么
	// 写），这次新增是因为name_reserve/global_settings/mod_dependences校验需要在Story
	// 对象创建之前就拿到同一份路径，包一层避免"test"这个字面量到处重复。等以后支持多
	// 主线剧情时再改这个方法内部的实现，调用方不用跟着改。
	static std::string GetMainStoryScriptPath();

	// jsonKey形如"building_mods"。返回该数组解析出的(id, 参数字符串)列表——每个数组元素
	// 按第一个空格切成两段,如"pengzhan --density 1.0"切成("pengzhan", "--density 1.0"),
	// 没有空格则参数为空串。参数字符串原样返回,格式/是否使用完全由mod自己的creator(收到
	// 这个字符串后决定怎么用)决定,Config不做任何解析。jsonKey不存在时返回空列表。
	static std::vector<std::pair<std::string, std::string>> GetConceptMods(const std::string& jsonKey);

private:
	static bool CheckFileFormat(const std::filesystem::path& filePath, const std::string& format);

	static std::string configDir;

	// mod根目录path -> 该目录下发现的、探测通过的dll绝对路径列表
	static std::unordered_map<std::string, std::vector<std::string>> dllPaths;

	// layout根目录path -> 该目录下发现的.layout绝对路径列表
	static std::unordered_map<std::string, std::vector<std::string>> layoutPaths;

	// resource根目录path -> 该目录下发现的.script绝对路径列表，结构和layoutPaths一致
	static std::unordered_map<std::string, std::vector<std::string>> resourcePaths;

	// main_story字段的原始值(ScriptModName)，ReadConfig时读取
	static std::string mainStoryScriptModName;

	// "<concept>_mods"这个json key -> 该数组解析出的(id, 参数字符串)列表
	static std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> conceptMods;
};
