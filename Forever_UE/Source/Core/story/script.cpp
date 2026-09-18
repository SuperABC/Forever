#include "story/script.h"

#include "story/event.h"
#include "story/dialog.h"
#include "story/change.h"

#include "common/error.h"
#include "common/json.h"

#include <fstream>
#include <filesystem>


using namespace std;

unordered_map<string, unordered_map<string, Milestone*>> Script::caches = {};

Script::Script(ScriptFactory* factory, const string& id) :
	mod(factory->CreateScript(id)),
	factory(factory),
	type(),
	name(),
	milestones(),
	actives() {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "Script " + id + " mod is null.\n");
	}

	type = mod->GetType();
	name = mod->GetName();
}

Script::~Script() {
	factory->DestroyScript(mod);
}

string Script::GetType() const {
	return type;
}

string Script::GetName() const {
	return name;
}

pair<bool, ValueType> Script::GetValue(const string& name) const {
	auto it = variables.find(name);
	if (it != variables.end()) {
		return { true, it->second };
	}
	return { false, ValueType() };
}

void Script::SetValue(const string& name, ValueType value) {
	variables[name] = value;
}

void Script::RemoveValue(const string& name) {
	variables.erase(name);
}

string Script::GetTask() const {
	string task;
	for (auto active : actives) {
		auto goal = active->content->GetGoal();
		if (goal.size() > 0) {
			task += "目标：" + goal + "\n";

			auto description = active->content->GetDescription();
			if (description.size() > 0) {
				task += "详细内容：" + description + "\n";
			}
		}
	}
	return task;
}

vector<ScriptAction>& Script::WrapScript(const Event* event, const vector<ScriptAction>& actions,
	const ScriptContext& context, PostHandle* post) {
	mod->WrapScript(event, actions, context, post);
	return mod->actionStack.back();
}

void Script::AutoPop() {
	mod->AutoPop();
}

void Script::ReadScript(const string& path) {
	if (path.empty()) {
		return;
	}
	if (caches.find(path) != caches.end()) {
		return;
	}

	if (!filesystem::exists(path)) {
		debugf("Warning: Script path does not exist: %s.\n", path.data());
		return;
	}

	JsonReader reader;
	JsonValue root;

	ifstream fin(path);
	if (!fin.is_open()) {
		THROW_EXCEPTION(IOException, "Failed to open file: " + path + ".\n");
	}

	if (reader.Parse(fin, root)) {
		unordered_map<string, Milestone*> parsed;
		for (auto milestone : root["milestones"]) {
			Milestone* content = new Milestone(
				milestone["milestone"].AsString(),
				BuildEvent(milestone["triggers"]),
				milestone["visible"].AsBool(),
				BuildExpression(milestone["drop"]),
				milestone["description"].AsString(),
				milestone["goal"].AsString(),
				BuildChanges(milestone["changes"]),
				BuildDialogs(milestone["dialogs"]),
				BuildSubsequences(milestone["subsequences"])
			);
			parsed.insert(make_pair(content->GetName(), content));
		}
		caches.insert(make_pair(path, move(parsed)));
	}
	else {
		fin.close();
		debugf("Json syntax error in %s: %s.\n", path.data(), reader.GetErrorMessages().data());
		THROW_EXCEPTION(JsonFormatException, "Json syntax error: " + reader.GetErrorMessages() + ".\n");
	}
	fin.close();
}

void Script::ReadMilestones(const string& path) {
	if (path.empty()) {
		return;
	}
	ReadScript(path);
	if (caches.find(path) == caches.end()) {
		debugf("Warning: Read script failed: %s.\n", path.data());
		return;
	}

	actives.clear();
	for (auto& [_, node] : milestones) {
		node.subsequents.clear();
		node.premise = 0;
	}

	for (auto& [msName, content] : caches[path]) {
		milestones[msName] = MilestoneNode(content);
	}
	for (auto& [name, node] : milestones) {
		for (auto subsequence : node.content->GetSubsequences()) {
			if (milestones.find(subsequence) == milestones.end()) continue;
			milestones[name].subsequents.push_back(&milestones[subsequence]);
			milestones[subsequence].premise++;
		}
	}
	for (auto& [_, node] : milestones) {
		if (node.premise == 0) {
			actives.push_back(&node);
		}
	}
}

vector<ScriptAction> Script::MatchEvent(Event* event, ScriptContext context, PostHandle* post) {
	context.self = this;

	vector<ScriptAction> actions;

	vector<MilestoneNode*> tmps;
	for (auto it = actives.begin(); it != actives.end(); ) {
		bool match = (*it)->content->MatchTrigger(event, context);

		if (match) {
			auto subsequents = (*it)->subsequents;
			for (auto subsequent : subsequents) {
				subsequent->premise--;
				if (subsequent->premise <= 0) {
					tmps.push_back(subsequent);
				}
			}

			for (auto change : (*it)->content->GetChanges()) {
				actions.push_back(static_cast<const Change*>(change));
			}
			for (auto dialog : (*it)->content->GetDialogs()) {
				actions.push_back(static_cast<const Dialog*>(dialog));
			}

			if ((*it)->content->DropSelf(context)) {
				it = actives.erase(it);
			}
			else {
				it++;
			}
		}
		else {
			it++;
		}
	}
	actives.insert(actives.end(), tmps.begin(), tmps.end());

	// WrapScript返回的是mod->actionStack这一层的引用（本体活在mod自己的堆上，见ScriptMod.h），
	// 这里立刻拷贝成Script/Core自己分配的vector（ScriptAction只是两个裸指针的variant，拷贝本身
	// 不跨堆），再AutoPop弹出mod那一层——拷贝必须在AutoPop之前，AutoPop一旦执行，前面这个引用
	// 就失效了。
	vector<ScriptAction> result(WrapScript(event, actions, context, post));
	AutoPop();
	return result;
}

void Script::DeactivateMilestone(const string& name) {
	for (auto it = actives.begin(); it != actives.end(); ) {
		if ((*it)->content->GetName() == name) {
			it = actives.erase(it);
			return;
		}
		else {
			it++;
		}
	}
}

void Script::ClearContext() {
	milestones.clear();
	actives.clear();
	variables.clear();
}

vector<Event*> Script::BuildEvent(const JsonValue& root) {
	vector<Event*> events;

	for (auto obj : root) {
		Event* event = nullptr;
		string type = obj["type"].AsString();

		if (type == "game_start") {
			event = new GameStartEvent();
		}
		else {
			// 阶段4占位：其余30种事件类型的JSON分发分支等该类型被点名实现时再补，见Script.md。
			THROW_EXCEPTION(RuntimeException, "Event type not implemented yet: " + type + ".\n");
		}

		event->SetCondition(BuildExpression(obj["condition"]));
		events.push_back(event);
	}

	return events;
}

vector<Change*> Script::BuildChanges(const JsonValue& root) {
	vector<Change*> changes;

	for (auto obj : root) {
		string type = obj["type"].AsString();
		Change* change = nullptr;

		if (type == "set_value") {
			auto variable = obj["variable"];
			auto value = obj["value"];
			if (variable.IsNull() || value.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing variable or value for set_value change.\n");
			}
			change = new SetValueChange(variable.AsString(), BuildExpression(value));
		}
		else if (type == "place_holder") {
			change = new PlaceHolderChange(BuildExpression(obj["label"]));
		}
		else {
			// 阶段4占位：其余40种变化类型的JSON分发分支等该类型被点名实现时再补，见Script.md。
			THROW_EXCEPTION(RuntimeException, "Change type not implemented yet: " + type + ".\n");
		}

		change->SetCondition(BuildExpression(obj["condition"]));
		changes.push_back(change);
	}

	return changes;
}

vector<Dialog*> Script::BuildDialogs(const JsonValue& root) {
	vector<Dialog*> dialogs;

	for (auto obj : root) {
		Dialog* dialog = new Dialog();

		dialog->SetCondition(BuildExpression(obj["condition"]));

		for (auto section : obj["list"]) {
			if (section.IsObject()) {
				dialog->AddDialog(BuildExpression(section["speaker"]), BuildExpression(section["content"]),
					BuildExpression(section["label"]), BuildExpression(section["voice"]));
			}
			else if (section.IsArray()) {
				vector<Option> options;
				for (auto item : section) {
					// 分支选项嵌套的dialogs/changes这里沿用老工程写法直接new出本体交给Option
					// 引用——和老工程一样，这些嵌套本体不在Milestone的顶层dialogs/changes列表
					// 里，所以也不会被Milestone析构时delete；本阶段test.json不含分支选项，
					// 这条路径未被实际执行到，真正的嵌套本体归属方式留到分支选项被点名实现
					// 时再确定，见Script.md。
					auto nestedChanges = BuildChanges(item["changes"]);
					options.emplace_back(BuildExpression(item["condition"]), BuildExpression(item["option"]),
						BuildDialogs(item["dialogs"]), vector<const Change*>(nestedChanges.begin(), nestedChanges.end()));
				}
				dialog->AddDialog(options);
			}
		}

		dialogs.push_back(dialog);
	}

	return dialogs;
}

Expression Script::BuildExpression(const JsonValue& root) {
	Expression expression;
	expression.Parse(root.AsString());
	return expression;
}

vector<string> Script::BuildSubsequences(const JsonValue& root) {
	vector<string> subsequences;
	for (auto obj : root) {
		subsequences.push_back(obj.AsString());
	}
	return subsequences;
}
