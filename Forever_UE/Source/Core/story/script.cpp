#include "story/script.h"

#include "story/event.h"
#include "story/dialog.h"
#include "story/change.h"
#include "common/error.h"
#include "common/json.h"

#include <fstream>
#include <filesystem>


using namespace std;

unordered_map<string, Script::FileCache> Script::caches = {};

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

ScriptMod* Script::GetMod() const {
	return mod;
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
		FileCache fileCache;
		for (auto milestone : root["milestones"]) {
			Milestone* content = new Milestone(
				milestone["milestone"].AsString(),
				BuildEvent(milestone["triggers"]),
				milestone["visible"].AsBool(),
				milestone["drop"].AsString(),
				milestone["description"].AsString(),
				milestone["goal"].AsString(),
				BuildChanges(milestone["changes"]),
				BuildDialogs(milestone["dialogs"]),
				BuildSubsequences(milestone["subsequences"])
			);
			fileCache.milestones.insert(make_pair(content->GetName(), content));
		}
		// name_reserve/global_settings/mod_dependences三个新增顶层字段——都是纯JSON
		// 数据，字段不存在就保持空，和BuildChanges里各处xxx.IsNull()判断可选字段是同一个
		// 既有约定，见script.md"主线剧情.script新增字段"一节。
		for (auto n : root["name_reserve"]) {
			fileCache.nameReserve.push_back(n.AsString());
		}
		if (!root["global_settings"]["time_flow_ratio"].IsNull()) {
			fileCache.globalSettings["time_flow_ratio"] = ValueType(root["global_settings"]["time_flow_ratio"].AsDouble());
		}
		for (auto dep : root["mod_dependences"]) {
			fileCache.modDependences.push_back(dep.AsString());
		}
		caches.insert(make_pair(path, move(fileCache)));
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

	for (auto& [msName, content] : caches[path].milestones) {
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

const vector<string>& Script::GetNameReserve(const string& path) {
	static const vector<string> empty;
	ReadScript(path);
	auto it = caches.find(path);
	return it != caches.end() ? it->second.nameReserve : empty;
}

const unordered_map<string, ValueType>& Script::GetGlobalSettings(const string& path) {
	static const unordered_map<string, ValueType> empty;
	ReadScript(path);
	auto it = caches.find(path);
	return it != caches.end() ? it->second.globalSettings : empty;
}

const vector<string>& Script::GetModDependences(const string& path) {
	static const vector<string> empty;
	ReadScript(path);
	auto it = caches.find(path);
	return it != caches.end() ? it->second.modDependences : empty;
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
		else if (type == "option_dialog") {
			event = new OptionDialogEvent(obj["name"].AsString(), obj["option"].AsString());
		}
		else if (type == "global_message") {
			// message可以省略(留空=收到任何全局消息都触发)，见GlobalMessageEvent::Match。
			event = new GlobalMessageEvent(obj["message"].AsString());
		}
		else if (type == "citizen_decease") {
			// name/reason都可以省略(留空=不限)，见CitizenDeceaseEvent::Match。
			event = new CitizenDeceaseEvent(obj["name"].AsString(), obj["reason"].AsString());
		}
		else {
			// 其余事件类型的JSON分发分支等该类型被点名实现时再补，见Script.md。
			THROW_EXCEPTION(RuntimeException, "Event type not implemented yet: " + type + ".\n");
		}

		event->SetCondition(obj["condition"].AsString());
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
			change = new SetValueChange(variable.AsString(), value.AsString());
		}
		else if (type == "place_holder") {
			change = new PlaceHolderChange(obj["label"].AsString());
		}
		else if (type == "debug_print") {
			change = new DebugPrintChange(obj["message"].AsString());
		}
		else if (type == "add_option") {
			auto name = obj["name"];
			auto option = obj["option"];
			if (name.IsNull() || option.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name or option for add_option change.\n");
			}
			change = new AddOptionChange(name.AsString(), option.AsString());
		}
		else if (type == "start_puzzle") {
			auto puzzle = obj["puzzle"];
			if (puzzle.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing puzzle for start_puzzle change.\n");
			}
			change = new StartPuzzleChange(puzzle.AsString());
		}
		else if (type == "enter_vehicle") {
			auto vehicle = obj["vehicle"];
			if (vehicle.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing vehicle for enter_vehicle change.\n");
			}
			change = new EnterVehicleChange(vehicle.AsString());
		}
		else if (type == "set_door") {
			// access可以写JSON布尔(true/false)，也可以写字符串表达式——AsString()把JSON布尔转成
			// "true"/"false"，执行时统一按布尔表达式求值。
			auto name = obj["name"];
			auto access = obj["access"];
			if (name.IsNull() || access.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name or access for set_door change.\n");
			}
			change = new SetDoorChange(name.AsString(), access.AsString());
		}
		else if (type == "global_message") {
			auto message = obj["message"];
			if (message.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing message for global_message change.\n");
			}
			change = new GlobalMessageChange(message.AsString());
		}
		else if (type == "for_range") {
			// {"var","from","to","step"(可选，默认1),"do":[changes]}，含两端；循环变量写进当前脚本自己的
			// 变量池($$self.<var>)，见AForeverFrameworkActor::ApplyChange的for_range分支。
			auto var = obj["var"];
			auto from = obj["from"];
			auto to = obj["to"];
			if (var.IsNull() || from.IsNull() || to.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing var, from or to for for_range change.\n");
			}
			vector<const Change*> nested;
			for (Change* child : BuildChanges(obj["do"])) nested.push_back(child);
			change = new ForRangeChange(var.AsString(), from.AsString(), to.AsString(), obj["step"].AsString(), move(nested));
		}
		else if (type == "launch_elevator") {
			auto building = obj["building"];
			auto elevator = obj["elevator"];
			auto command = obj["command"];
			if (elevator.IsNull() || command.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing elevator or command for launch_elevator change.\n");
			}
			change = new LaunchElevatorChange(building.AsString(), elevator.AsString(), command.AsString());
		}
		else if (type == "remove_option") {
			auto name = obj["name"];
			auto option = obj["option"];
			if (name.IsNull() || option.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name or option for remove_option change.\n");
			}
			change = new RemoveOptionChange(name.AsString(), option.AsString());
		}
		else if (type == "npc_navigate") {
			auto name = obj["name"];
			auto destination = obj["destination"];
			if (name.IsNull() || destination.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name or destination for npc_navigate change.\n");
			}
			change = new NPCNavigateChange(name.AsString(), destination.AsString());
		}
		else if (type == "change_control") {
			auto name = obj["name"];
			if (name.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name for change_control change.\n");
			}
			change = new ChangeControlChange(name.AsString());
		}
		else if (type == "register_door") {
			auto name = obj["name"];
			auto citizen = obj["citizen"];
			if (name.IsNull() || citizen.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name or citizen for register_door change.\n");
			}
			// pass省略时按true(允许通过)。
			change = new RegisterDoorChange(name.AsString(), citizen.AsString(), obj["pass"].AsString());
		}
		else {
			// 其余变化类型的JSON分发分支等该类型被点名实现时再补，见Script.md。
			THROW_EXCEPTION(RuntimeException, "Change type not implemented yet: " + type + ".\n");
		}

		change->SetCondition(obj["condition"].AsString());
		changes.push_back(change);
	}

	return changes;
}

vector<Dialog*> Script::BuildDialogs(const JsonValue& root) {
	vector<Dialog*> dialogs;

	for (auto obj : root) {
		Dialog* dialog = new Dialog();

		dialog->SetCondition(obj["condition"].AsString());

		for (auto section : obj["list"]) {
			if (section.IsObject()) {
				dialog->AddDialog(section["speaker"].AsString(), section["content"].AsString(),
					section["label"].AsString(), section["voice"].AsString());
			}
			else if (section.IsArray()) {
				vector<Option> options;
				for (auto item : section) {
					// 分支选项嵌套的dialogs/changes直接new出本体交给Option引用——这些嵌套本体
					// 不在Milestone的顶层dialogs/changes列表里，所以也不会被Milestone析构时
					// delete；目前test.json不含分支选项，这条路径未被实际执行到，真正的嵌套
					// 本体归属方式留到分支选项被点名实现时再确定，见dialog.md。
					auto nestedChanges = BuildChanges(item["changes"]);
					options.emplace_back(item["condition"].AsString(), item["option"].AsString(),
						BuildDialogs(item["dialogs"]), vector<const Change*>(nestedChanges.begin(), nestedChanges.end()));
				}
				dialog->AddDialog(options);
			}
		}

		dialogs.push_back(dialog);
	}

	return dialogs;
}

vector<string> Script::BuildSubsequences(const JsonValue& root) {
	vector<string> subsequences;
	for (auto obj : root) {
		subsequences.push_back(obj.AsString());
	}
	return subsequences;
}
