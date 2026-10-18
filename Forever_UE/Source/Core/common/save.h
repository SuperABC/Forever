#pragma once

#include "common/save_archive.h"
#include "common/mod_state.h"
#include "common/json.h"

#include <string>
#include <vector>
#include <utility>


// 存档/读档的Core侧编排：manifest(用到的mod/脚本/布局、版本、计数器)、段版本表、mod状态的
// JSON读写实现、加载前校验、把存档的mod集合应用到当前Config。UE层(ForeverSaveSubsystem)负责
// 文件、压缩和界面。详见save.md。

// 游戏版本字符串，只写进manifest用于显示/排查，不参与兼容性判断。发版时手动改。
#define SAVE_GAME_VERSION "0.1.0"

// 段名(各域自己的段名定义在各自的*_save.h里)。
#define SAVE_SECTION_MANIFEST "manifest"
// 全部Script的运行时状态(script_save.h)。读档时这一段存在就跳过开局的game_start广播，并恢复
// 门/市民/车辆上脚本加过的交互选项。
#define SAVE_SECTION_SCRIPTS "world.scripts"
// UE侧状态(JSON，由ForeverSaveSubsystem读写)：当前操控的市民/车辆等。
#define SAVE_SECTION_UE "ue"

// 段版本表：每个段的当前版本和当前代码还能读的最旧版本。改某个段的数据布局时只升那一段的
// current，并在save.cpp里补一个从旧版本升上来的迁移；比minimum还旧、或者比current还新的段拒绝读。
struct SaveSectionVersion {
	const char* name;
	int current;
	int minimum;
	// 必须存在的段：缺了就拒绝读档(比如只有manifest的P0旧存档)。
	bool required = true;
};
const std::vector<SaveSectionVersion>& GetSaveSectionVersions();

// manifest里的一条mod记录：按(jsonKey, modId)记(同一个id可能在不同concept里重名)，args是
// config.json里的参数字符串，stateVersion是保存时这个mod的GetStateVersion()(没有存活实例时为0)，
// counter是保存时这个mod类的命名计数器(-1表示没有/不恢复)。
struct SaveModRecord {
	std::string conceptKey; // config.json里的"<concept>_mods"
	std::string modId;
	std::string args;
	int stateVersion = 0;
	int counter = -1;
};

struct SaveManifest {
	std::string gameVersion;
	std::string mainStory;                // main_story(脚本mod名)
	std::vector<std::string> storyScripts; // Story screen的脚本，只存脚本名
	std::vector<SaveModRecord> mods;
	std::vector<std::string> scripts;     // 用到的全部脚本名(资源检查用)
	std::vector<std::string> layouts;     // 用到的全部.layout模板名(资源检查用)

	JsonValue ToJson() const;
	static bool FromJson(const JsonValue& json, SaveManifest& out, std::string& error);
};

// StateWriter/StateReader的JSON实现(宿主侧)，mod的SaveState/LoadState就写读这里。
class JsonStateWriter : public StateWriter {
public:
	JsonStateWriter();

	virtual void SetInt(const char* key, int64_t value) override;
	virtual void SetDouble(const char* key, double value) override;
	virtual void SetBool(const char* key, bool value) override;
	virtual void SetString(const char* key, const char* value) override;
	virtual void SetBytes(const char* key, const void* data, int64_t size) override;
	virtual void BeginObject(const char* key) override;
	virtual void EndObject() override;
	virtual void BeginArray(const char* key) override;
	virtual void EndArray() override;

	// 写完之后取结果(未闭合的层级会自动闭合)。
	JsonValue Finish();

private:
	void Put(const char* key, const JsonValue& value);
	void Close();

	std::vector<std::pair<std::string, JsonValue>> stack;
};

class JsonStateReader : public StateReader {
public:
	JsonStateReader(const JsonValue& root, int savedVersion);

	virtual int GetSavedVersion() const override;
	virtual bool Has(const char* key) const override;
	virtual int64_t GetInt(const char* key, int64_t fallback = 0) const override;
	virtual double GetDouble(const char* key, double fallback = 0.0) const override;
	virtual bool GetBool(const char* key, bool fallback = false) const override;
	virtual const char* GetString(const char* key, const char* fallback = "") const override;
	virtual int64_t GetBytes(const char* key, void* out, int64_t outCapacity) const override;
	virtual bool EnterObject(const char* key) override;
	virtual void ExitObject() override;
	virtual bool EnterArray(const char* key) override;
	virtual void ExitArray() override;
	virtual int GetArraySize() const override;
	virtual bool EnterArrayElement(int index) override;
	virtual int64_t GetIntAt(int index, int64_t fallback = 0) const override;
	virtual double GetDoubleAt(int index, double fallback = 0.0) const override;
	virtual const char* GetStringAt(int index, const char* fallback = "") const override;

private:
	const JsonValue* Current() const;
	const JsonValue* Member(const char* key) const;
	const JsonValue* Element(int index) const;

	JsonValue root;
	int savedVersion;
	std::vector<const JsonValue*> stack;
	mutable std::string scratch;
};

class Map;
class Populace;
class Society;
class Traffic;
class Story;
class Player;

// 存档时传给SaveGame的当前世界(还没生成的域传nullptr，对应的段就不写)。
struct SaveWorld {
	const Map* map = nullptr;
	const Populace* populace = nullptr;
	const Society* society = nullptr;
	const Traffic* traffic = nullptr;
	const Story* story = nullptr;
	const Player* player = nullptr;
};

class SaveGame {
public:
	// 收集当前进程的manifest：启用的mod(按Config)、各mod的命名计数器和状态版本(按Factory里的
	// 存活实例)、脚本名、布局模板名(按map里各建筑的楼层模板快照)。
	static SaveManifest BuildManifest(const SaveWorld& world);

	// 生成存档"压缩体"里的分段容器：manifest + 地图4段(map_save.h) + 人口(populace_save.h) +
	// 社会(society_save.h) + 交通(traffic_save.h) + 脚本状态(script_save.h) + 玩家(player_save.h)，
	// 段列表见save.md。
	static SaveArchive BuildArchive(const SaveWorld& world);

	// 加载前校验(不改任何状态)：段版本、manifest解析、mod是否都被已发现的DLL提供、依赖、
	// 脚本/布局资源是否都在。返回错误列表(空=通过)，manifest解析成功时写进outManifest。
	static std::vector<std::string> ValidateArchive(const SaveArchive& archive, SaveManifest& outManifest);

	// 读档前的配置快照：ApplyManifestConfig之后校验失败时用它恢复。
	struct ConfigSnapshot {
		std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> conceptMods;
		std::vector<std::string> storyScripts;
		std::string mainStory;
	};

	// 把存档用到的mod集合/剧情脚本设成当前Config(只改内存)并Registry::ReloadModArgs()，返回改之前的快照。
	static ConfigSnapshot ApplyManifestConfig(const SaveManifest& manifest);
	static void RestoreConfig(const ConfigSnapshot& snapshot);

	// mod是否真的注册进了Factory(DLL已加载且已启用)。Registry只在第一次Get()时加载DLL，游戏内读档
	// 需要一个启动时没加载过的DLL就会在这里报出来(只能重启游戏)。必须在ApplyManifestConfig之后调用。
	static std::vector<std::string> ValidateModsRegistered(const SaveManifest& manifest);

	// mod状态版本检查：存档里某个mod的stateVersion比当前mod的GetStateVersion()还新就报错。只对
	// stateVersion>0的mod检查(要临时创建一个实例取版本)，必须在ApplyManifestConfig之后调用。
	static std::vector<std::string> ValidateModStateVersions(const SaveManifest& manifest);

	// 把每个mod类的命名计数器恢复成存档里的值(临时创建一个实例、SetInstanceCounter、再销毁)。
	// 必须在读档重建完所有mod实例之后调用。
	static void RestoreInstanceCounters(const SaveManifest& manifest);
};
