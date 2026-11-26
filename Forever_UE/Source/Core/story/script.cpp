#include "story/script.h"

#include "story/event.h"
#include "story/dialog.h"
#include "story/change.h"
#include "common/error.h"
#include "common/json.h"
#include "common/config.h"

#include <fstream>
#include <filesystem>


using namespace std;

unordered_map<string, Script::FileCache> Script::caches = {};
int Script::liveCount = 0;

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
	liveCount++;
}

Script::~Script() {
	factory->DestroyScript(mod);
	liveCount--;
}

bool Script::ClearCache() {
	if (liveCount > 0) {
		debugf(DebugLevel::Warning, "Script::ClearCache skipped, %d script(s) still alive.\n", liveCount);
		return false;
	}
	for (auto& [path, fileCache] : caches) {
		for (auto& [milestoneName, milestone] : fileCache.milestones) delete milestone;
	}
	caches.clear();
	return true;
}

string Script::GetType() const {
	return type;
}

string Script::GetName() const {
	return name;
}

void Script::SetOwner(ScriptOwnerKind kind, void* owner, void* parent) {
	ownerKind = kind;
	this->owner = owner;
	ownerParent = parent;
}

void Script::SetOwnerParent(void* parent) { ownerParent = parent; }
ScriptOwnerKind Script::GetOwnerKind() const { return ownerKind; }
void* Script::GetOwner() const { return owner; }
void* Script::GetOwnerParent() const { return ownerParent; }
const std::string& Script::GetStableKey() const { return stableKey; }
void Script::SetStableKey(std::string key) const { stableKey = std::move(key); }

ScriptMod* Script::GetMod() const {
	return mod;
}

const ValueType* Script::FindValue(const string& name) const {
	auto it = variables.find(name);
	return it != variables.end() ? &it->second : nullptr;
}

void Script::SetValue(const string& name, const ValueType& value) {
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
		debugf(DebugLevel::Warning, "Script path does not exist: %s.\n", path.data());
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
		// "names"是name_reserve的别名(老工程剧情脚本的写法)，两个字段都写则合并。
		for (auto n : root["names"]) {
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
		debugf(DebugLevel::Error, "Json syntax error in %s: %s.\n", path.data(), reader.GetErrorMessages().data());
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
		debugf(DebugLevel::Warning, "Read script failed: %s.\n", path.data());
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

	// WrapScript返回的是mod->actionStack这一层的引用（本体活在mod自己的堆上，见script_mod.h），
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
		else if (type == "weather_change") {
			// weather可以省略(留空=任何天气变化都触发)，见WeatherChangeEvent::Match。
			event = new WeatherChangeEvent(obj["weather"].AsString());
		}
		else if (type == "speaking_finish") {
			// label可以省略(留空=任何带label的台词播完都触发)，见SpeakingFinishEvent::Match。
			event = new SpeakingFinishEvent(obj["label"].AsString());
		}
		else if (type == "enter_zone" || type == "leave_zone") {
			// 位置类事件的字段都可以省略(留空=不限)，见Dependence/story/event.cpp MatchLocationField。
			if (type == "enter_zone") event = new EnterZoneEvent(obj["zone"].AsString());
			else event = new LeaveZoneEvent(obj["zone"].AsString());
		}
		else if (type == "enter_building" || type == "leave_building") {
			if (type == "enter_building") event = new EnterBuildingEvent(obj["zone"].AsString(), obj["building"].AsString());
			else event = new LeaveBuildingEvent(obj["zone"].AsString(), obj["building"].AsString());
		}
		else if (type == "enter_room" || type == "leave_room") {
			if (type == "enter_room") event = new EnterRoomEvent(obj["zone"].AsString(), obj["building"].AsString(), obj["room"].AsString());
			else event = new LeaveRoomEvent(obj["zone"].AsString(), obj["building"].AsString(), obj["room"].AsString());
		}
		else if (type == "enter_stop" || type == "leave_stop") {
			// stop可以省略(留空=不限站点)，见EnterStopEvent::Match。
			if (type == "enter_stop") event = new EnterStopEvent(obj["stop"].AsString());
			else event = new LeaveStopEvent(obj["stop"].AsString());
		}
		else if (type == "get_asset" || type == "lose_asset") {
			// JSON里的"type"已经是事件类型本身，资产类型的过滤字段叫asset_type；asset_type/result都可以省略
			// (留空=不限)，见GetAssetEvent::Match。脚本里读事件字段仍然是$$local.type/$$local.result。
			if (type == "get_asset") event = new GetAssetEvent(obj["asset_type"].AsString(), obj["result"].AsString());
			else event = new LoseAssetEvent(obj["asset_type"].AsString(), obj["result"].AsString());
		}
		else if (type == "use_asset") {
			// 没有过滤字段(只发给资产自己的脚本)，脚本里读$$local.asset/type/user。
			event = new UseAssetEvent(std::string());
		}
		else if (type == "throw_asset") {
			// asset_type可以省略(留空=不限)，见ThrowAssetEvent::Match。脚本里读$$local.type。
			event = new ThrowAssetEvent(obj["asset_type"].AsString());
		}
		else if (type == "weapon_fire") {
			// weapon_type可以省略(留空=不限)，见WeaponFireEvent::Match。脚本里读$$local.hit等，字段见event.h。
			event = new WeaponFireEvent(obj["weapon_type"].AsString());
		}
		else if (type == "projectile_hit") {
			event = new ProjectileHitEvent();
		}
		else if (type == "enter_vehicle" || type == "leave_vehicle") {
			// vehicle/mode都可以省略(留空=不限)，见EnterVehicleEvent::Match。mode是"drive"/"ride"。
			if (type == "enter_vehicle") event = new EnterVehicleEvent(obj["vehicle"].AsString(), obj["mode"].AsString());
			else event = new LeaveVehicleEvent(obj["vehicle"].AsString(), obj["mode"].AsString());
		}
		else if (type == "puzzle_result") {
			// 没有任何过滤字段，小游戏的结果字段在脚本里用local.<展平名>读，见PuzzleResultEvent::SetFromJson。
			event = new PuzzleResultEvent();
		}
		else if (type == "npc_arrive") {
			// name/address都可以省略(留空=不限)，见NpcArriveEvent::Match；address是房间地址(Room::GetAddress())。
			event = new NpcArriveEvent(obj["name"].AsString(), obj["address"].AsString());
		}
		else if (type == "task_complete" || type == "task_failed") {
			// task/destination都可以省略(留空=不限)，见TaskCompleteEvent::Match；事件只发给派任务的组织的脚本。
			if (type == "task_complete") event = new TaskCompleteEvent(obj["task"].AsString(), obj["destination"].AsString());
			else event = new TaskFailedEvent(obj["task"].AsString(), obj["destination"].AsString());
		}
		else if (type == "meeting_start") {
			// party/address都可以省略(留空=不限)，见MeetingStartEvent::Match；事件只发给该党党员自己的Scheduler脚本。
			event = new MeetingStartEvent(obj["party"].AsString(), obj["address"].AsString());
		}
		else if (type == "meeting_end") {
			event = new MeetingEndEvent(obj["party"].AsString(), obj["address"].AsString());
		}
		else if (type == "execute_task") {
			// task/destination都可以省略(留空=不限)，见ExecuteTaskEvent::Match；事件只发给被派任务的员工的Job脚本。
			event = new ExecuteTaskEvent(obj["task"].AsString(), obj["destination"].AsString());
		}
		else if (type == "vehicle_arrive") {
			// vehicle/address都可以省略(留空=不限)，见VehicleArriveEvent::Match；事件只发给这辆车自己的script。
			event = new VehicleArriveEvent(obj["vehicle"].AsString(), obj["address"].AsString());
		}
		else if (type == "time_up") {
			// name可以省略(留空=这个脚本的任何计时器到时都触发)，见TimeUpEvent::Match。
			event = new TimeUpEvent(obj["name"].AsString());
		}
		else if (type == "citizen_decease") {
			// name/reason都可以省略(留空=不限)，见CitizenDeceaseEvent::Match。
			// killer/weapon/part同样可省略(留空=不限)；damage/visible/location只在运行时事件里有，不能做过滤。
			event = new CitizenDeceaseEvent(obj["name"].AsString(), obj["reason"].AsString(), obj["killer"].AsString(),
				obj["weapon"].AsString(), obj["part"].AsString(), 0.0, false, std::string());
		}
		else if (type == "citizen_injured") {
			// name/attacker/weapon/part都可以省略(留空=不限)；damage/health/destination只在运行时事件里有，不能做过滤。
			event = new CitizenInjuredEvent(obj["name"].AsString(), obj["attacker"].AsString(), obj["weapon"].AsString(),
				obj["part"].AsString());
		}
		else if (type == "flee_start") {
			event = new FleeStartEvent(obj["name"].AsString(), obj["from"].AsString());
		}
		else if (type == "flee_end") {
			event = new FleeEndEvent(obj["name"].AsString(), obj["from"].AsString(), obj["reason"].AsString());
		}
		else {
			// 其余事件类型的JSON分发分支等该类型被点名实现时再补，见Core/story/script.md。
			THROW_EXCEPTION(RuntimeException, "Event type not implemented yet: " + type + ".\n");
		}

		event->SetCondition(obj["condition"].AsString());
		events.push_back(event);
	}

	return events;
}

vector<Change*> Script::BuildChanges(const JsonValue& root) {
	vector<Change*> changes;

	// 解析中途抛异常时释放已经new出的Change(for_range的子项由ForRangeChange析构释放)，正常返回前release
	struct Guard {
		vector<Change*>& owned;
		bool released = false;
		~Guard() {
			if (!released) for (Change* item : owned) delete item;
		}
	} guard{ changes };

	for (auto obj : root) {
		string type = obj["type"].AsString();
		Change* change = nullptr;

		// 类型名先换成种类(名字表在Dependence/story/change.cpp)，再switch：一步到对应分支，不用逐个比字符串。
		ChangeKind kind;
		if (!FindChangeKind(type, kind)) {
			THROW_EXCEPTION(RuntimeException, "Unknown change type: " + type + ".\n");
		}
		switch (kind) {
		case ChangeKind::SetValue: {
			auto variable = obj["variable"];
			auto value = obj["value"];
			if (variable.IsNull() || value.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing variable or value for set_value change.\n");
			}
			change = new SetValueChange(variable.AsString(), value.AsString());
			break;
		}
		case ChangeKind::GlobalSetting: {
			auto setting = obj["setting"];
			auto value = obj["value"];
			if (setting.IsNull() || value.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing setting or value for global_setting change.\n");
			}
			change = new GlobalSettingChange(setting.AsString(), value.AsString());
			break;
		}
		case ChangeKind::PlaceHolder: {
			change = new PlaceHolderChange(obj["label"].AsString());
			break;
		}
		case ChangeKind::DebugPrint: {
			change = new DebugPrintChange(obj["message"].AsString());
			break;
		}
		case ChangeKind::AddOption: {
			auto name = obj["name"];
			auto option = obj["option"];
			if (name.IsNull() || option.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name or option for add_option change.\n");
			}
			// category可选：citizen/vehicle/door/elevator/asset，指定后只有对应的域去找目标，不指定就每个域各试一遍(慢)。
			auto category = obj["category"];
			string categoryText = category.IsNull() ? string() : category.AsString();
			if (!categoryText.empty() && categoryText != "citizen" && categoryText != "vehicle" && categoryText != "door"
				&& categoryText != "elevator" && categoryText != "asset") {
				THROW_EXCEPTION(RuntimeException, "Unknown category for add_option change (citizen/vehicle/door/elevator/asset).\n");
			}
			change = new AddOptionChange(name.AsString(), option.AsString(), categoryText);
			break;
		}
		case ChangeKind::StartPuzzle: {
			auto puzzle = obj["puzzle"];
			if (puzzle.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing puzzle for start_puzzle change.\n");
			}
			// param可选：剧情脚本传给小游戏的启动参数(表达式)，缺省为空串
			auto param = obj["param"];
			change = new StartPuzzleChange(puzzle.AsString(), param.IsNull() ? string() : param.AsString());
			break;
		}
		case ChangeKind::OpenShop: {
			change = new OpenShopChange();
			break;
		}
		case ChangeKind::PayWage: {
			auto amount = obj["amount"];
			if (amount.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing amount for pay_wage change.\n");
			}
			change = new PayWageChange(amount.AsString());
			break;
		}
		case ChangeKind::EnterVehicle: {
			auto vehicle = obj["vehicle"];
			if (vehicle.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing vehicle for enter_vehicle change.\n");
			}
			change = new EnterVehicleChange(vehicle.AsString());
			break;
		}
		case ChangeKind::VehicleCommand: {
			auto vehicle = obj["vehicle"];
			auto command = obj["command"];
			if (vehicle.IsNull() || command.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing vehicle or command for vehicle_command change.\n");
			}
			change = new VehicleCommandChange(vehicle.AsString(), command.AsString());
			break;
		}
		case ChangeKind::VehicleNavigate: {
			auto vehicle = obj["vehicle"];
			auto destination = obj["destination"];
			if (vehicle.IsNull() || destination.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing vehicle or destination for vehicle_navigate change.\n");
			}
			change = new VehicleNavigateChange(vehicle.AsString(), destination.AsString());
			break;
		}
		case ChangeKind::ShowRoute: {
			auto target = obj["name"];
			if (target.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name for show_route change.\n");
			}
			change = new ShowRouteChange(target.AsString());
			break;
		}
		case ChangeKind::VisualizeNavigation: {
			auto address = obj["address"];
			if (address.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing address for visualize_navigation change.\n");
			}
			change = new VisualizeNavigationChange(address.AsString());
			break;
		}
		case ChangeKind::BoardVehicle: {
			auto vehicle = obj["vehicle"];
			if (vehicle.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing vehicle for board_vehicle change.\n");
			}
			change = new BoardVehicleChange(vehicle.AsString());
			break;
		}
		case ChangeKind::LeaveVehicle: {
			auto vehicle = obj["vehicle"];
			if (vehicle.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing vehicle for leave_vehicle change.\n");
			}
			change = new LeaveVehicleChange(vehicle.AsString());
			break;
		}
		case ChangeKind::ForceLeave: {
			// vehicle可以省略(留空=玩家当前所在的任何载具)，见ForceLeaveChange。
			auto vehicle = obj["vehicle"];
			change = new ForceLeaveChange(vehicle.IsNull() ? string() : vehicle.AsString());
			break;
		}
		case ChangeKind::SetDoor: {
			// access可以写JSON布尔(true/false)，也可以写字符串表达式——AsString()把JSON布尔转成
			// "true"/"false"，执行时统一按布尔表达式求值。
			auto name = obj["name"];
			auto access = obj["access"];
			if (name.IsNull() || access.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name or access for set_door change.\n");
			}
			change = new SetDoorChange(name.AsString(), access.AsString());
			break;
		}
		case ChangeKind::GlobalMessage: {
			auto message = obj["message"];
			if (message.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing message for global_message change.\n");
			}
			// category可选：接收方的脚本类别，逗号分隔多个(main/organization/job/citizen/vehicle/door/elevator/building/room/zone/asset)，
			// 缺省=所有脚本都收。写错的类别直接报错。
			auto category = obj["category"];
			string categoryText = category.IsNull() ? string() : category.AsString();
			{
				static const char* const kCategories[] = { "main", "organization", "job", "citizen", "vehicle", "door",
					"elevator", "building", "room", "zone", "asset" };
				size_t start = 0;
				while (!categoryText.empty() && start <= categoryText.size()) {
					size_t comma = categoryText.find(',', start);
					string item = categoryText.substr(start, comma == string::npos ? string::npos : comma - start);
					bool known = false;
					for (const char* candidate : kCategories) known |= (item == candidate);
					if (!known) {
						THROW_EXCEPTION(RuntimeException, "Unknown category for global_message change (main/organization/job/citizen/vehicle/door/elevator/building/room/zone/asset, comma separated).\n");
					}
					if (comma == string::npos) break;
					start = comma + 1;
				}
			}
			change = new GlobalMessageChange(message.AsString(), categoryText);
			break;
		}
		case ChangeKind::ChangeWeather: {
			// weather是表达式，求值结果是天气名：sunny/cloudy/overcast/light_rain/heavy_rain/light_snow/heavy_snow。
			auto weather = obj["weather"];
			if (weather.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing weather for change_weather change.\n");
			}
			change = new ChangeWeatherChange(weather.AsString());
			break;
		}
		case ChangeKind::ForRange: {
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
			break;
		}
		case ChangeKind::PickAsset: {
			// asset可以省略(=执行这个Change的脚本所属的资产)。
			change = new PickAssetChange(obj["asset"].AsString());
			break;
		}
		case ChangeKind::SpawnEffect: {
			// effect/scale/asset/position/normal都可以省略：不生成特效、缩放1、执行这个Change的脚本所属的资产的位置、朝上。
			change = new SpawnEffectChange(obj["effect"].AsString(), obj["scale"].AsString(), obj["asset"].AsString(),
				obj["position"].AsString(), obj["normal"].AsString());
			break;
		}
		case ChangeKind::SoundEffect: {
			// position/asset/volume/pitch都可以省略：执行脚本所属资产的位置、音量和音调倍率1。
			change = new SoundEffectChange(obj["sound"].AsString(), obj["position"].AsString(), obj["asset"].AsString(),
				obj["volume"].AsString(), obj["pitch"].AsString());
			break;
		}
		case ChangeKind::SpawnActor: {
			// direction/scale可以省略：不旋转、缩放1。
			change = new SpawnActorChange(obj["actor"].AsString(), obj["position"].AsString(), obj["direction"].AsString(),
				obj["scale"].AsString());
			break;
		}
		case ChangeKind::SingleDamage: {
			// source/weapon/part只用于击杀信息，可以省略。
			change = new SingleDamageChange(obj["target"].AsString(), obj["damage"].AsString(), obj["source"].AsString(),
				obj["weapon"].AsString(), obj["part"].AsString());
			break;
		}
		case ChangeKind::AddSatiety: {
			// target省略=玩家当前操控的市民。
			change = new AddSatietyChange(obj["target"].AsString(), obj["amount"].AsString());
			break;
		}
		case ChangeKind::AreaDamage: {
			// position省略=执行脚本所属资产的位置；occlusion省略=false；power省略=1。
			change = new AreaDamageChange(obj["position"].AsString(), obj["radius"].AsString(), obj["damage"].AsString(),
				obj["occlusion"].AsString(), obj["power"].AsString(), obj["source"].AsString(), obj["weapon"].AsString());
			break;
		}
		case ChangeKind::DestroyAsset: {
			// asset可以省略(=执行这个Change的脚本所属的资产)。
			change = new DestroyAssetChange(obj["asset"].AsString());
			break;
		}
		case ChangeKind::GrantAsset:
		case ChangeKind::DeductAsset:
		case ChangeKind::DropAsset: {
			// JSON里的"type"已经是Change类型本身，资产类型的字段叫asset_type。
			auto assetType = obj["asset_type"];
			if (assetType.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing asset_type for " + type + " change.\n");
			}
			// count可以省略(=1)。
			string count = obj["count"].AsString();
			if (kind == ChangeKind::GrantAsset) change = new GrantAssetChange(assetType.AsString(), count, obj["slot"].AsString()); // slot可以省略
			else if (kind == ChangeKind::DeductAsset) change = new DeductAssetChange(assetType.AsString(), count);
			else change = new DropAssetChange(assetType.AsString(), count);
			break;
		}
		case ChangeKind::NPCRevive: {
			auto name = obj["name"];
			if (name.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name for npc_revive change.\n");
			}
			// health可以省略(=满血)。
			change = new NPCReviveChange(name.AsString(), obj["health"].AsString());
			break;
		}
		case ChangeKind::NPCTeleport: {
			auto name = obj["name"];
			auto destination = obj["destination"];
			if (name.IsNull() || destination.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name or destination for npc_teleport change.\n");
			}
			change = new NPCTeleportChange(name.AsString(), destination.AsString());
			break;
		}
		case ChangeKind::SpawnNpc: {
			auto name = obj["name"];
			auto gender = obj["gender"];
			auto birthday = obj["birthday"];
			if (name.IsNull() || gender.IsNull() || birthday.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name, gender or birthday for spawn_npc change.\n");
			}
			vector<string> jobs;
			for (auto job : obj["jobs"]) jobs.push_back(job.AsString());
			// 其余字段都可选；目前真正生效的只有name/gender/birthday/home/scheduler，见story.md"spawn_npc"。
			change = new SpawnNpcChange(obj["avatar"].AsString(), name.AsString(), gender.AsString(), birthday.AsString(),
				obj["height"].AsString(), obj["weight"].AsString(), obj["nick"].AsString(), obj["deposit"].AsString(),
				obj["phone"].AsString(), obj["home"].AsString(), jobs, obj["scheduler"].AsString(), obj["economy"].AsString());
			break;
		}
		case ChangeKind::CreateTimer: {
			auto name = obj["name"];
			auto time = obj["time"];
			if (name.IsNull() || time.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name or time for create_timer change.\n");
			}
			// category/label可以省略：省略时计时器归创建它的脚本自己，见story.md"计时器"一节。
			// mode省略=absolute，可选absolute/relative/real/next，见CreateTimerChange::SetMode。
			change = new CreateTimerChange(name.AsString(), time.AsString(), obj["category"].AsString(), obj["label"].AsString(),
				obj["mode"].AsString());
			break;
		}
		case ChangeKind::JumpTime: {
			auto time = obj["time"];
			if (time.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing time for jump_time change.\n");
			}
			// mode同create_timer(absolute/relative/next，不支持real)，省略=absolute。
			change = new JumpTimeChange(time.AsString(), obj["mode"].AsString());
			break;
		}
		case ChangeKind::RemoveTimer: {
			auto name = obj["name"];
			if (name.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name for remove_timer change.\n");
			}
			change = new RemoveTimerChange(name.AsString());
			break;
		}
		case ChangeKind::LaunchElevator: {
			auto building = obj["building"];
			auto elevator = obj["elevator"];
			auto command = obj["command"];
			if (elevator.IsNull() || command.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing elevator or command for launch_elevator change.\n");
			}
			change = new LaunchElevatorChange(building.AsString(), elevator.AsString(), command.AsString());
			break;
		}
		case ChangeKind::RemoveOption: {
			auto name = obj["name"];
			auto option = obj["option"];
			if (name.IsNull() || option.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name or option for remove_option change.\n");
			}
			// category可选：citizen/vehicle/door/elevator/asset，指定后只有对应的域去找目标，不指定就每个域各试一遍(慢)。
			auto category = obj["category"];
			string categoryText = category.IsNull() ? string() : category.AsString();
			if (!categoryText.empty() && categoryText != "citizen" && categoryText != "vehicle" && categoryText != "door"
				&& categoryText != "elevator" && categoryText != "asset") {
				THROW_EXCEPTION(RuntimeException, "Unknown category for remove_option change (citizen/vehicle/door/elevator/asset).\n");
			}
			change = new RemoveOptionChange(name.AsString(), option.AsString(), categoryText);
			break;
		}
		case ChangeKind::NPCNavigate: {
			auto name = obj["name"];
			auto destination = obj["destination"];
			if (name.IsNull() || destination.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name or destination for npc_navigate change.\n");
			}
			// speed可省略(空=1.0)：走路速度倍率表达式，载入时检查语法。
			string speed = obj["speed"].AsString();
			{
				Expression checker;
				if (!speed.empty() && !checker.Parse(speed)) {
					THROW_EXCEPTION(RuntimeException, "Invalid expression \"" + speed + "\" in speed of npc_navigate change.\n");
				}
			}
			change = new NPCNavigateChange(name.AsString(), destination.AsString(), move(speed));
			break;
		}
		case ChangeKind::CitizenFlee: {
			auto name = obj["name"];
			auto from = obj["from"];
			if (name.IsNull() || from.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name or from for citizen_flee change.\n");
			}
			change = new CitizenFleeChange(name.AsString(), from.AsString());
			break;
		}
		case ChangeKind::AssignTask: {
			auto destination = obj["destination"];
			if (destination.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing destination for assign_task change.\n");
			}
			// task/vehicles/worker都可省略：task空=无标签，vehicles空=只走路，worker空=自动挑闲置员工。
			// vehicles是JSON数组，每一项各自是一个表达式(求值结果是载具category，空串=走路)，载入时就检查语法，写错立刻报错。
			vector<string> vehicles;
			for (auto vehicle : obj["vehicles"]) {
				string source = vehicle.AsString();
				Expression checker;
				if (!source.empty() && !checker.Parse(source)) {
					THROW_EXCEPTION(RuntimeException, "Invalid expression \"" + source + "\" in vehicles of assign_task change.\n");
				}
				vehicles.push_back(move(source));
			}
			string speed = obj["speed"].AsString();
			{
				Expression checker;
				if (!speed.empty() && !checker.Parse(speed)) {
					THROW_EXCEPTION(RuntimeException, "Invalid expression \"" + speed + "\" in speed of assign_task change.\n");
				}
			}
			change = new AssignTaskChange(obj["task"].AsString(), destination.AsString(), move(vehicles), obj["worker"].AsString(), move(speed));
			break;
		}
		case ChangeKind::ChangeControl: {
			auto name = obj["name"];
			if (name.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name for change_control change.\n");
			}
			change = new ChangeControlChange(name.AsString());
			break;
		}
		case ChangeKind::RecordObject: {
			// {"object":"zone|building|room","kind":mod type(可空),"policy":"first|random"(可空),"variable":结果变量}，
			// 见Map::ApplyChange。
			auto object = obj["object"];
			auto variable = obj["variable"];
			if (object.IsNull() || variable.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing object or variable for record_object change.\n");
			}
			change = new RecordObjectChange(object.AsString(), obj["kind"].AsString(), obj["policy"].AsString(), variable.AsString());
			break;
		}
		case ChangeKind::RegisterDoor: {
			auto name = obj["name"];
			auto citizen = obj["citizen"];
			if (name.IsNull() || citizen.IsNull()) {
				THROW_EXCEPTION(RuntimeException, "Missing name or citizen for register_door change.\n");
			}
			// pass省略时按true(允许通过)。
			change = new RegisterDoorChange(name.AsString(), citizen.AsString(), obj["pass"].AsString());
			break;
		}
		default: {
			// 其余种类(有类、名字表里也有，但还没写JSON解析)等该类型被点名实现时再补，见Core/story/script.md。
			THROW_EXCEPTION(RuntimeException, "Change type not implemented yet: " + type + ".\n");
		}
		}

		change->SetCondition(obj["condition"].AsString());
		changes.push_back(change);
	}

	guard.released = true;
	return changes;
}

vector<Dialog*> Script::BuildDialogs(const JsonValue& root) {
	vector<Dialog*> dialogs;

	for (auto obj : root) {
		Dialog* dialog = new Dialog();

		dialog->SetCondition(obj["condition"].AsString());

		for (auto section : obj["list"]) {
			if (section.IsObject()) {
				// background(秒，可选)：大于0时是后台对话，见Dependence/story/dialog.h Section。
				float background = section["background"].IsNull() ? 0.f : section["background"].AsFloat();
				dialog->AddDialog(section["speaker"].AsString(), section["content"].AsString(),
					section["label"].AsString(), section["voice"].AsString(), background);
			}
			else if (section.IsArray()) {
				vector<Option> options;
				for (auto item : section) {
					// 分支选项嵌套的dialogs/changes直接new出本体交给Option引用——这些嵌套本体
					// 不在Milestone的顶层列表里，由包含该分支段的Dialog析构时释放，见dialog.md。
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

Script* MakeScript(ScriptFactory* factory, const string& scriptModName, const vector<string>& milestoneNames) {
	if (!factory) return nullptr;
	if (scriptModName.empty() && milestoneNames.empty()) return nullptr;
	Script* script = new Script(factory, scriptModName.empty() ? string(kDefaultScriptModId) : scriptModName);
	for (const string& milestoneName : milestoneNames) {
		script->ReadMilestones(Config::GetScriptPath(milestoneName));
	}
	return script;
}
