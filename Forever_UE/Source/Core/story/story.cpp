#include "story/story.h"

#include <algorithm>

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
	delete globalScript;
	globalScript = nullptr;
}

void Story::Init() {
	// ScriptModName从config.json的"main_story"字段读，不由Core硬编码。
	string scriptModName = Config::GetMainStoryScriptModName();
	if (scriptModName.empty() || !scriptFactory.CheckRegistered(scriptModName)) {
		debugf("Warning: main_story script mod not configured or not registered, Story::Init skipped.\n");
		return;
	}

	systemScript = new Script(&scriptFactory, scriptModName);
	globalScript = new Script(&scriptFactory, scriptModName);

	mainScript = new Script(&scriptFactory, scriptModName);
	// 主线读取config.json的story_scripts里的全部文件(没有配置时退回test.script)，每个文件的
	// milestones合并进同一个主线Script，见Config::GetMainStoryScriptPaths()。
	for (const string& path : Config::GetMainStoryScriptPaths()) {
		mainScript->ReadMilestones(path);
	}
}

void Story::BroadcastGameStart(const function<void(const vector<ScriptAction>&, const ScriptContext&)>& onActions,
	PostHandle* post) {
	if (!mainScript) return;

	GameStartEvent event;
	ScriptContext context;
	context.self = mainScript;
	context.system = systemScript;
	context.global = globalScript;
	context.local = &event;

	auto scriptActions = mainScript->MatchEvent(&event, context, post);
	onActions(scriptActions, context);
}

bool Story::ApplyChange(const Change* change, const ScriptContext& context) {
	if (auto setValue = dynamic_cast<const SetValueChange*>(change)) {
		// 变量名的前缀规则和读取($$x)同一条(ResolveVariableName)：self.前缀写当前脚本自己的变量，
		// 没有前缀写全局变量；system.(系统变量，只由C++写)和local.(事件字段)脚本不能写，忽略并警告。
		string key;
		VARIABLE_SCOPE scope = ResolveVariableName(setValue->GetVariable(), key);
		Container* target = scope == VARIABLE_SCOPE_SELF ? context.self
			: scope == VARIABLE_SCOPE_GLOBAL ? context.global : nullptr;
		if (target) target->SetValue(key, EvaluateExpression(setValue->GetValue(), context));
		else if (scope == VARIABLE_SCOPE_SYSTEM || scope == VARIABLE_SCOPE_LOCAL) {
			debugf("Warning: set_value cannot write read-only variable %s (system./local. are read-only in scripts).\n",
				setValue->GetVariable().c_str());
		}
		return true;
	}

	return false;
}

void Story::Tick(const Time& currentTime, bool crossedDay, PostHandle* post) {
	// 占位：Story域目前没有每帧逻辑。
}

Script* Story::GetSystemScript() const {
	return systemScript;
}

Script* Story::GetGlobalScript() const {
	return globalScript;
}

void Story::EraseTimer(const string& name, const string& ownerKey) {
	auto it = timerIndex.find({ name, ownerKey });
	if (it != timerIndex.end()) {
		gameTimers.erase(it->second);
		timerIndex.erase(it);
	}
	realTimers.erase(remove_if(realTimers.begin(), realTimers.end(), [&](const StoryTimer& timer) {
		return timer.name == name && timer.ownerKey == ownerKey;
		}), realTimers.end());
}

void Story::CreateTimer(const string& name, const Time& target, const string& ownerKey) {
	EraseTimer(name, ownerKey);
	StoryTimer timer;
	timer.target = target;
	timer.name = name;
	timer.ownerKey = ownerKey;
	timerIndex[{ name, ownerKey }] = gameTimers.emplace(target, std::move(timer));
}

void Story::CreateRealTimer(const string& name, double seconds, const string& ownerKey) {
	EraseTimer(name, ownerKey);
	StoryTimer timer;
	timer.name = name;
	timer.ownerKey = ownerKey;
	timer.realTime = true;
	timer.remaining = seconds;
	realTimers.push_back(std::move(timer));
}

void Story::AdvanceRealTimers(double deltaSeconds) {
	for (StoryTimer& timer : realTimers) timer.remaining -= deltaSeconds;
}

void Story::RemoveTimer(const string& name, const string& ownerKey) {
	EraseTimer(name, ownerKey);
}

vector<StoryTimer> Story::PopExpiredTimers(const Time& now, size_t maxCount) {
	vector<StoryTimer> expired;
	// 游戏时间计时器已按到时时刻排好，只弹队首；同一时刻的保持创建先后。
	while (expired.size() < maxCount && !gameTimers.empty() && gameTimers.begin()->first <= now) {
		auto it = gameTimers.begin();
		timerIndex.erase({ it->second.name, it->second.ownerKey });
		expired.push_back(std::move(it->second));
		gameTimers.erase(it);
	}
	// 现实时间计时器排在后面，按剩余时间(越负越早到)。
	vector<StoryTimer> realExpired;
	for (size_t i = 0; i < realTimers.size() && expired.size() + realExpired.size() < maxCount;) {
		if (realTimers[i].remaining <= 0.0) {
			realExpired.push_back(std::move(realTimers[i]));
			realTimers.erase(realTimers.begin() + i);
		}
		else {
			i++;
		}
	}
	stable_sort(realExpired.begin(), realExpired.end(), [](const StoryTimer& a, const StoryTimer& b) {
		return a.remaining < b.remaining;
		});
	for (StoryTimer& timer : realExpired) expired.push_back(std::move(timer));
	return expired;
}

vector<StoryTimer> Story::GetTimers() const {
	vector<StoryTimer> all;
	all.reserve(gameTimers.size() + realTimers.size());
	for (const auto& [target, timer] : gameTimers) all.push_back(timer);
	for (const StoryTimer& timer : realTimers) all.push_back(timer);
	return all;
}

void Story::ClearTimers() {
	gameTimers.clear();
	timerIndex.clear();
	realTimers.clear();
}

void Story::RestoreTimer(const StoryTimer& timer) {
	if (timer.realTime) CreateRealTimer(timer.name, timer.remaining, timer.ownerKey);
	else CreateTimer(timer.name, timer.target, timer.ownerKey);
}

Script* Story::GetMainScript() const {
	return mainScript;
}
