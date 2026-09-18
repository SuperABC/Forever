#include "story/story.h"

#include "story/event.h"
#include "story/change.h"

#include "common/config.h"
#include "common/utility.h"

#include <filesystem>


using namespace std;

namespace {
	unordered_map<string, string> ToArgsMap(const vector<pair<string, string>>& entries) {
		unordered_map<string, string> args;
		for (auto& [id, arg] : entries) {
			args[id] = arg;
		}
		return args;
	}

	// 主线剧情/系统变量池这两个Script当前阶段都不需要区分具体mod内容，统一走config.json
	// "script_mods"里已经配好的"empty"id（Forever_Mod/Empty提供），test.json的内容和
	// 具体挂载的ScriptMod无关，只是需要"随便一个能创建出来的ScriptMod"作为Script的壳。
	constexpr const char* kEmptyScriptId = "empty";

	// Resource/Story/test.json：相对config.json所在目录(Resource/Config/)的固定相对路径，
	// 这次不复活老工程Config::GetStories()那一整套多剧情路径管理，见Story.md。
	string GetTestScriptPath() {
		filesystem::path configDir = Config::GetConfigDir();
		return (configDir / ".." / "Story" / "test.json").string();
	}
}

Story::Story() {
	modLoader.RegisterConcept<ScriptFactory>(Config::GetMods(), "RegisterModScripts", "FinishModScripts", &scriptFactory);
	scriptFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("script_mods")));
}

Story::~Story() {
	for (auto script : mainScripts) {
		delete script;
	}
	mainScripts.clear();
	delete systemScript;
	systemScript = nullptr;
}

void Story::Init() {
	if (!scriptFactory.CheckRegistered(kEmptyScriptId)) {
		debugf("Warning: script mod '%s' not registered, Story::Init skipped.\n", kEmptyScriptId);
		return;
	}

	systemScript = new Script(&scriptFactory, kEmptyScriptId);

	Script* mainScript = new Script(&scriptFactory, kEmptyScriptId);
	mainScript->ReadMilestones(GetTestScriptPath());
	mainScripts.push_back(mainScript);
}

void Story::BroadcastGameStart(const function<void(const vector<ScriptAction>&, const ScriptContext&)>& onActions,
	PostHandle* post) {
	GameStartEvent event;
	for (auto script : mainScripts) {
		ScriptContext context;
		context.self = script;
		context.system = systemScript;
		context.local = &event;

		auto scriptActions = script->MatchEvent(&event, context, post);
		onActions(scriptActions, context);
	}
}

void Story::ApplyChange(const Change* change, const ScriptContext& context) {
	if (auto setValue = dynamic_cast<const SetValueChange*>(change)) {
		if (context.self) {
			context.self->SetValue(setValue->GetVariable(), setValue->GetValue().EvaluateValue(context));
		}
		return;
	}

	// 阶段4占位：其余41种变化类型的执行分支等该类型被点名实现时再补，见Story.md。
	debugf("Warning: change type %s not implemented yet.\n", change->GetType().data());
}

Script* Story::GetSystemScript() const {
	return systemScript;
}

const vector<Script*>& Story::GetMainScripts() const {
	return mainScripts;
}
