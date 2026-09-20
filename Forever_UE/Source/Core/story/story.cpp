#include "story/story.h"

#include "story/event.h"
#include "story/change.h"
#include "common/utility.h"

#include "common/config.h"
#include "common/registry.h"


using namespace std;

Story::Story() :
	scriptFactory(Registry::Get().GetScriptFactory()) {
}

Story::~Story() {
	delete mainScript;
	mainScript = nullptr;
	delete systemScript;
	systemScript = nullptr;
}

void Story::Init() {
	// ScriptModName不再由Core硬编码，从config.json的"main_story"字段读——不替Story做
	// "用哪个ScriptMod"这个决策，和Job/Organization这次的Script配置是同一个原则。
	string scriptModName = Config::GetMainStoryScriptModName();
	if (scriptModName.empty() || !scriptFactory.CheckRegistered(scriptModName)) {
		debugf("Warning: main_story script mod not configured or not registered, Story::Init skipped.\n");
		return;
	}

	systemScript = new Script(&scriptFactory, scriptModName);

	mainScript = new Script(&scriptFactory, scriptModName);
	// test.script的具体存放位置由config.json的resource_path决定，这里只写bare文件名，
	// 见Config::GetScriptPath()的说明。
	mainScript->ReadMilestones(Config::GetScriptPath("test"));
}

void Story::BroadcastGameStart(const function<void(const vector<ScriptAction>&, const ScriptContext&)>& onActions,
	PostHandle* post) {
	if (!mainScript) return;

	GameStartEvent event;
	ScriptContext context;
	context.self = mainScript;
	context.system = systemScript;
	context.local = &event;

	auto scriptActions = mainScript->MatchEvent(&event, context, post);
	onActions(scriptActions, context);
}

void Story::ApplyChange(const Change* change, const ScriptContext& context) {
	if (auto setValue = dynamic_cast<const SetValueChange*>(change)) {
		if (context.self) {
			context.self->SetValue(setValue->GetVariable(), EvaluateExpression(setValue->GetValue(), context));
		}
		return;
	}

	// 阶段4占位：其余41种变化类型的执行分支等该类型被点名实现时再补，见Story.md。
	debugf("Warning: change type %s not implemented yet.\n", change->GetType().data());
}

Script* Story::GetSystemScript() const {
	return systemScript;
}

Script* Story::GetMainScript() const {
	return mainScript;
}
