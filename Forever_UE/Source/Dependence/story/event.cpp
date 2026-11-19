#include "event.h"

#include <cstdio>


using namespace std;

namespace {
	// vec3 local：name="(x, y, z)"字符串 + name.x/name.y/name.z浮点分量。
	void StoreVec3(map<string, ValueType>& fields, const string& name, double x, double y, double z) {
		char text[128];
		snprintf(text, sizeof(text), "(%g, %g, %g)", x, y, z);
		fields[name] = ValueType(string(text));
		fields[name + ".x"] = ValueType(x);
		fields[name + ".y"] = ValueType(y);
		fields[name + ".z"] = ValueType(z);
	}

	// 纯字符串字段的匹配（位置/站点/载具/资产事件共用，名字沿用最初的位置类事件）：触发条件里的字段是表达式，留空表示不限。
	bool MatchLocationField(const string& expr, const string& actual, const ScriptContext& context) {
		return expr.empty() || ToString(EvaluateExpression(expr, context)) == actual;
	}
}

Event::Event() :
	condition() {

}

Event::~Event() {

}

bool Event::Match(Event* e, const ScriptContext& context) {
	if (!e) return false;
	return GetType() == e->GetType();
}

const std::string& Event::GetCondition() const {
	return condition;
}

void Event::SetCondition(const std::string& condition) {
	this->condition = condition;
}

pair<bool, ValueType> Event::GetLocalValue(const string& name) const {
	return { false, ValueType() };
}

GameStartEvent::GameStartEvent() {

}

GameStartEvent::~GameStartEvent() {

}

const string& GameStartEvent::GetType() const {
	static const string type = "game_start";
	return type;
}

GlobalMessageEvent::GlobalMessageEvent(std::string message) :
	message(move(message)) {

}

GlobalMessageEvent::~GlobalMessageEvent() {

}

const string& GlobalMessageEvent::GetType() const {
	static const string type = "global_message";
	return type;
}

bool GlobalMessageEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<GlobalMessageEvent*>(e);
	if (!message.empty() && ToString(EvaluateExpression(message, context)) != other->message) return false;
	return true;
}

pair<bool, ValueType> GlobalMessageEvent::GetLocalValue(const string& key) const {
	if (key == "message") return { true, ValueType(message) };
	return { false, ValueType() };
}

void GlobalMessageEvent::SetMessage(std::string message) {
	this->message = move(message);
}

const std::string& GlobalMessageEvent::GetMessage() const {
	return message;
}

OptionDialogEvent::OptionDialogEvent(std::string name, std::string option) :
	name(move(name)), option(move(option)) {

}

OptionDialogEvent::~OptionDialogEvent() {

}

const string& OptionDialogEvent::GetType() const {
	static const string type = "option_dialog";
	return type;
}

bool OptionDialogEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<OptionDialogEvent*>(e);
	if (!name.empty() && ToString(EvaluateExpression(name, context)) != other->name) return false;
	if (!option.empty() && ToString(EvaluateExpression(option, context)) != other->option) return false;
	return true;
}

pair<bool, ValueType> OptionDialogEvent::GetLocalValue(const string& key) const {
	if (key == "name") return { true, ValueType(name) };
	if (key == "option") return { true, ValueType(option) };
	return { false, ValueType() };
}

void OptionDialogEvent::SetName(std::string name) {
	this->name = move(name);
}

const std::string& OptionDialogEvent::GetName() const {
	return name;
}

void OptionDialogEvent::SetOption(std::string option) {
	this->option = move(option);
}

const std::string& OptionDialogEvent::GetOption() const {
	return option;
}

SpeakingFinishEvent::SpeakingFinishEvent(std::string label) :
	label(move(label)) {

}

SpeakingFinishEvent::~SpeakingFinishEvent() {

}

const string& SpeakingFinishEvent::GetType() const {
	static const string type = "speaking_finish";
	return type;
}

bool SpeakingFinishEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<SpeakingFinishEvent*>(e);
	if (!label.empty() && ToString(EvaluateExpression(label, context)) != other->label) return false;
	return true;
}

pair<bool, ValueType> SpeakingFinishEvent::GetLocalValue(const string& key) const {
	if (key == "label") return { true, ValueType(label) };
	return { false, ValueType() };
}

void SpeakingFinishEvent::SetLabel(std::string label) {
	this->label = move(label);
}

const std::string& SpeakingFinishEvent::GetLabel() const {
	return label;
}

EnterZoneEvent::EnterZoneEvent(std::string zone) :
	zone(move(zone)) {

}

EnterZoneEvent::~EnterZoneEvent() {

}

const string& EnterZoneEvent::GetType() const {
	static const string type = "enter_zone";
	return type;
}

bool EnterZoneEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<EnterZoneEvent*>(e);
	if (!MatchLocationField(zone, other->zone, context)) return false;
	return true;
}

pair<bool, ValueType> EnterZoneEvent::GetLocalValue(const string& key) const {
	if (key == "zone") return { true, ValueType(zone) };
	return { false, ValueType() };
}

void EnterZoneEvent::SetZone(std::string zone) {
	this->zone = move(zone);
}

const std::string& EnterZoneEvent::GetZone() const {
	return zone;
}

LeaveZoneEvent::LeaveZoneEvent(std::string zone) :
	zone(move(zone)) {

}

LeaveZoneEvent::~LeaveZoneEvent() {

}

const string& LeaveZoneEvent::GetType() const {
	static const string type = "leave_zone";
	return type;
}

bool LeaveZoneEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<LeaveZoneEvent*>(e);
	if (!MatchLocationField(zone, other->zone, context)) return false;
	return true;
}

pair<bool, ValueType> LeaveZoneEvent::GetLocalValue(const string& key) const {
	if (key == "zone") return { true, ValueType(zone) };
	return { false, ValueType() };
}

void LeaveZoneEvent::SetZone(std::string zone) {
	this->zone = move(zone);
}

const std::string& LeaveZoneEvent::GetZone() const {
	return zone;
}

EnterBuildingEvent::EnterBuildingEvent(std::string zone, std::string building) :
	zone(move(zone)), building(move(building)) {

}

EnterBuildingEvent::~EnterBuildingEvent() {

}

const string& EnterBuildingEvent::GetType() const {
	static const string type = "enter_building";
	return type;
}

bool EnterBuildingEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<EnterBuildingEvent*>(e);
	if (!MatchLocationField(zone, other->zone, context)) return false;
	if (!MatchLocationField(building, other->building, context)) return false;
	return true;
}

pair<bool, ValueType> EnterBuildingEvent::GetLocalValue(const string& key) const {
	if (key == "zone") return { true, ValueType(zone) };
	if (key == "building") return { true, ValueType(building) };
	return { false, ValueType() };
}

void EnterBuildingEvent::SetZone(std::string zone) {
	this->zone = move(zone);
}

const std::string& EnterBuildingEvent::GetZone() const {
	return zone;
}

void EnterBuildingEvent::SetBuilding(std::string building) {
	this->building = move(building);
}

const std::string& EnterBuildingEvent::GetBuilding() const {
	return building;
}

LeaveBuildingEvent::LeaveBuildingEvent(std::string zone, std::string building) :
	zone(move(zone)), building(move(building)) {

}

LeaveBuildingEvent::~LeaveBuildingEvent() {

}

const string& LeaveBuildingEvent::GetType() const {
	static const string type = "leave_building";
	return type;
}

bool LeaveBuildingEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<LeaveBuildingEvent*>(e);
	if (!MatchLocationField(zone, other->zone, context)) return false;
	if (!MatchLocationField(building, other->building, context)) return false;
	return true;
}

pair<bool, ValueType> LeaveBuildingEvent::GetLocalValue(const string& key) const {
	if (key == "zone") return { true, ValueType(zone) };
	if (key == "building") return { true, ValueType(building) };
	return { false, ValueType() };
}

void LeaveBuildingEvent::SetZone(std::string zone) {
	this->zone = move(zone);
}

const std::string& LeaveBuildingEvent::GetZone() const {
	return zone;
}

void LeaveBuildingEvent::SetBuilding(std::string building) {
	this->building = move(building);
}

const std::string& LeaveBuildingEvent::GetBuilding() const {
	return building;
}

EnterRoomEvent::EnterRoomEvent(std::string zone, std::string building, std::string room) :
	zone(move(zone)), building(move(building)), room(move(room)) {

}

EnterRoomEvent::~EnterRoomEvent() {

}

const string& EnterRoomEvent::GetType() const {
	static const string type = "enter_room";
	return type;
}

bool EnterRoomEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<EnterRoomEvent*>(e);
	if (!MatchLocationField(zone, other->zone, context)) return false;
	if (!MatchLocationField(building, other->building, context)) return false;
	if (!MatchLocationField(room, other->room, context)) return false;
	return true;
}

pair<bool, ValueType> EnterRoomEvent::GetLocalValue(const string& key) const {
	if (key == "zone") return { true, ValueType(zone) };
	if (key == "building") return { true, ValueType(building) };
	if (key == "room") return { true, ValueType(room) };
	return { false, ValueType() };
}

void EnterRoomEvent::SetZone(std::string zone) {
	this->zone = move(zone);
}

const std::string& EnterRoomEvent::GetZone() const {
	return zone;
}

void EnterRoomEvent::SetBuilding(std::string building) {
	this->building = move(building);
}

const std::string& EnterRoomEvent::GetBuilding() const {
	return building;
}

void EnterRoomEvent::SetRoom(std::string room) {
	this->room = move(room);
}

const std::string& EnterRoomEvent::GetRoom() const {
	return room;
}

LeaveRoomEvent::LeaveRoomEvent(std::string zone, std::string building, std::string room) :
	zone(move(zone)), building(move(building)), room(move(room)) {

}

LeaveRoomEvent::~LeaveRoomEvent() {

}

const string& LeaveRoomEvent::GetType() const {
	static const string type = "leave_room";
	return type;
}

bool LeaveRoomEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<LeaveRoomEvent*>(e);
	if (!MatchLocationField(zone, other->zone, context)) return false;
	if (!MatchLocationField(building, other->building, context)) return false;
	if (!MatchLocationField(room, other->room, context)) return false;
	return true;
}

pair<bool, ValueType> LeaveRoomEvent::GetLocalValue(const string& key) const {
	if (key == "zone") return { true, ValueType(zone) };
	if (key == "building") return { true, ValueType(building) };
	if (key == "room") return { true, ValueType(room) };
	return { false, ValueType() };
}

void LeaveRoomEvent::SetZone(std::string zone) {
	this->zone = move(zone);
}

const std::string& LeaveRoomEvent::GetZone() const {
	return zone;
}

void LeaveRoomEvent::SetBuilding(std::string building) {
	this->building = move(building);
}

const std::string& LeaveRoomEvent::GetBuilding() const {
	return building;
}

void LeaveRoomEvent::SetRoom(std::string room) {
	this->room = move(room);
}

const std::string& LeaveRoomEvent::GetRoom() const {
	return room;
}

PuzzleResultEvent::PuzzleResultEvent() {

}

PuzzleResultEvent::~PuzzleResultEvent() {

}

const string& PuzzleResultEvent::GetType() const {
	static const string type = "puzzle_result";
	return type;
}

bool PuzzleResultEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	return true;
}

pair<bool, ValueType> PuzzleResultEvent::GetLocalValue(const string& key) const {
	auto it = fields.find(key);
	if (it == fields.end()) return { false, ValueType() };
	return { true, it->second };
}

bool PuzzleResultEvent::SetFromJson(const JsonValue& root) {
	fields.clear();
	if (!root.IsObject()) return false;
	Flatten("", root);
	return true;
}

const std::map<std::string, ValueType>& PuzzleResultEvent::GetFields() const {
	return fields;
}

void PuzzleResultEvent::Flatten(const string& name, const JsonValue& value) {
	if (value.IsObject()) {
		for (const string& member : value.GetMembers()) {
			Flatten(name.empty() ? member : name + "." + member, value[member]);
		}
	}
	else if (value.IsArray()) {
		for (unsigned int i = 0; i < value.size(); i++) {
			Flatten(name + "[" + to_string(i) + "]", value[i]);
		}
	}
	else if (name.empty()) {
		return;
	}
	else if (value.IsBool()) {
		fields[name] = ValueType(value.AsBool());
	}
	else if (value.IsIntegral()) {
		fields[name] = ValueType(value.AsInt());
	}
	else if (value.IsDouble()) {
		fields[name] = ValueType(value.AsDouble());
	}
	else if (value.IsString()) {
		fields[name] = ValueType(value.AsString());
	}
}

TransactionResultEvent::TransactionResultEvent(std::string result, std::string name) :
	result(move(result)), name(move(name)) {

}

TransactionResultEvent::~TransactionResultEvent() {

}

const string& TransactionResultEvent::GetType() const {
	static const string type = "transaction_result";
	return type;
}

void TransactionResultEvent::SetResult(std::string result) {
	this->result = move(result);
}

const std::string& TransactionResultEvent::GetResult() const {
	return result;
}

void TransactionResultEvent::SetName(std::string name) {
	this->name = move(name);
}

const std::string& TransactionResultEvent::GetName() const {
	return name;
}

TimeUpEvent::TimeUpEvent(std::string name) :
	name(move(name)) {

}

TimeUpEvent::~TimeUpEvent() {

}

const string& TimeUpEvent::GetType() const {
	static const string type = "time_up";
	return type;
}

bool TimeUpEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<TimeUpEvent*>(e);
	if (!name.empty() && ToString(EvaluateExpression(name, context)) != other->name) return false;
	return true;
}

pair<bool, ValueType> TimeUpEvent::GetLocalValue(const string& key) const {
	if (key == "name") return { true, ValueType(name) };
	return { false, ValueType() };
}

void TimeUpEvent::SetName(std::string name) {
	this->name = move(name);
}

const std::string& TimeUpEvent::GetName() const {
	return name;
}

NpcArriveEvent::NpcArriveEvent(std::string name, std::string address) :
	name(move(name)), address(move(address)) {

}

NpcArriveEvent::~NpcArriveEvent() {

}

const string& NpcArriveEvent::GetType() const {
	static const string type = "npc_arrive";
	return type;
}

bool NpcArriveEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<NpcArriveEvent*>(e);
	if (!name.empty() && ToString(EvaluateExpression(name, context)) != other->name) return false;
	if (!address.empty() && ToString(EvaluateExpression(address, context)) != other->address) return false;
	return true;
}

pair<bool, ValueType> NpcArriveEvent::GetLocalValue(const string& key) const {
	if (key == "name") return { true, ValueType(name) };
	if (key == "address") return { true, ValueType(address) };
	return { false, ValueType() };
}

void NpcArriveEvent::SetName(std::string name) {
	this->name = move(name);
}

const std::string& NpcArriveEvent::GetName() const {
	return name;
}

void NpcArriveEvent::SetAddress(std::string address) {
	this->address = move(address);
}

const std::string& NpcArriveEvent::GetAddress() const {
	return address;
}

ExecuteTaskEvent::ExecuteTaskEvent(std::string task, std::string destination) :
	task(move(task)), destination(move(destination)) {

}

ExecuteTaskEvent::~ExecuteTaskEvent() {

}

const string& ExecuteTaskEvent::GetType() const {
	static const string type = "execute_task";
	return type;
}

bool ExecuteTaskEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<ExecuteTaskEvent*>(e);
	if (!task.empty() && ToString(EvaluateExpression(task, context)) != other->task) return false;
	if (!destination.empty() && ToString(EvaluateExpression(destination, context)) != other->destination) return false;
	return true;
}

pair<bool, ValueType> ExecuteTaskEvent::GetLocalValue(const string& key) const {
	if (key == "task") return { true, ValueType(task) };
	if (key == "destination") return { true, ValueType(destination) };
	return { false, ValueType() };
}

TaskCompleteEvent::TaskCompleteEvent(std::string task, std::string destination, std::string worker, std::string origin) :
	task(move(task)), destination(move(destination)), worker(move(worker)), origin(move(origin)) {

}

TaskCompleteEvent::~TaskCompleteEvent() {

}

const string& TaskCompleteEvent::GetType() const {
	static const string type = "task_complete";
	return type;
}

bool TaskCompleteEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<TaskCompleteEvent*>(e);
	if (!task.empty() && ToString(EvaluateExpression(task, context)) != other->task) return false;
	if (!destination.empty() && ToString(EvaluateExpression(destination, context)) != other->destination) return false;
	return true;
}

pair<bool, ValueType> TaskCompleteEvent::GetLocalValue(const string& key) const {
	if (key == "task") return { true, ValueType(task) };
	if (key == "destination") return { true, ValueType(destination) };
	if (key == "worker") return { true, ValueType(worker) };
	if (key == "origin") return { true, ValueType(origin) };
	return { false, ValueType() };
}

TaskFailedEvent::TaskFailedEvent(std::string task, std::string destination, std::string worker, std::string origin, std::string reason) :
	task(move(task)), destination(move(destination)), worker(move(worker)), origin(move(origin)), reason(move(reason)) {

}

TaskFailedEvent::~TaskFailedEvent() {

}

const string& TaskFailedEvent::GetType() const {
	static const string type = "task_failed";
	return type;
}

bool TaskFailedEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<TaskFailedEvent*>(e);
	if (!task.empty() && ToString(EvaluateExpression(task, context)) != other->task) return false;
	if (!destination.empty() && ToString(EvaluateExpression(destination, context)) != other->destination) return false;
	return true;
}

pair<bool, ValueType> TaskFailedEvent::GetLocalValue(const string& key) const {
	if (key == "task") return { true, ValueType(task) };
	if (key == "destination") return { true, ValueType(destination) };
	if (key == "worker") return { true, ValueType(worker) };
	if (key == "origin") return { true, ValueType(origin) };
	if (key == "reason") return { true, ValueType(reason) };
	return { false, ValueType() };
}

VehicleArriveEvent::VehicleArriveEvent(std::string vehicle, std::string address) :
	vehicle(move(vehicle)), address(move(address)) {

}

VehicleArriveEvent::~VehicleArriveEvent() {

}

const string& VehicleArriveEvent::GetType() const {
	static const string type = "vehicle_arrive";
	return type;
}

bool VehicleArriveEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<VehicleArriveEvent*>(e);
	if (!vehicle.empty() && ToString(EvaluateExpression(vehicle, context)) != other->vehicle) return false;
	if (!address.empty() && ToString(EvaluateExpression(address, context)) != other->address) return false;
	return true;
}

pair<bool, ValueType> VehicleArriveEvent::GetLocalValue(const string& key) const {
	if (key == "vehicle") return { true, ValueType(vehicle) };
	if (key == "address") return { true, ValueType(address) };
	return { false, ValueType() };
}

void VehicleArriveEvent::SetVehicle(std::string vehicle) {
	this->vehicle = move(vehicle);
}

const std::string& VehicleArriveEvent::GetVehicle() const {
	return vehicle;
}

void VehicleArriveEvent::SetAddress(std::string address) {
	this->address = move(address);
}

const std::string& VehicleArriveEvent::GetAddress() const {
	return address;
}

NPCMeetEvent::NPCMeetEvent(std::string npc) :
	npc(move(npc)) {

}

NPCMeetEvent::~NPCMeetEvent() {

}

const string& NPCMeetEvent::GetType() const {
	static const string type = "npc_meet";
	return type;
}

void NPCMeetEvent::SetNPC(std::string npc) {
	this->npc = move(npc);
}

const std::string& NPCMeetEvent::GetNPC() const {
	return npc;
}

CitizenBornEvent::CitizenBornEvent(std::string name) :
	name(move(name)) {

}

CitizenBornEvent::~CitizenBornEvent() {

}

const string& CitizenBornEvent::GetType() const {
	static const string type = "citizen_born";
	return type;
}

void CitizenBornEvent::SetName(std::string name) {
	this->name = move(name);
}

const std::string& CitizenBornEvent::GetName() const {
	return name;
}

CitizenDeceaseEvent::CitizenDeceaseEvent(std::string name, std::string reason) :
	name(move(name)), reason(move(reason)) {

}

CitizenDeceaseEvent::CitizenDeceaseEvent(std::string name, std::string reason, std::string killer, std::string weapon,
	std::string part, double damage, bool visible, std::string location) :
	name(move(name)), reason(move(reason)), killer(move(killer)), weapon(move(weapon)), part(move(part)),
	damage(damage), visible(visible), location(move(location)) {

}

CitizenDeceaseEvent::~CitizenDeceaseEvent() {

}

const string& CitizenDeceaseEvent::GetType() const {
	static const string type = "citizen_decease";
	return type;
}

bool CitizenDeceaseEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<CitizenDeceaseEvent*>(e);
	if (!name.empty() && ToString(EvaluateExpression(name, context)) != other->name) return false;
	if (!reason.empty() && ToString(EvaluateExpression(reason, context)) != other->reason) return false;
	if (!killer.empty() && ToString(EvaluateExpression(killer, context)) != other->killer) return false;
	if (!weapon.empty() && ToString(EvaluateExpression(weapon, context)) != other->weapon) return false;
	if (!part.empty() && ToString(EvaluateExpression(part, context)) != other->part) return false;
	return true;
}

pair<bool, ValueType> CitizenDeceaseEvent::GetLocalValue(const string& key) const {
	if (key == "name") return { true, ValueType(name) };
	if (key == "reason") return { true, ValueType(reason) };
	if (key == "killer") return { true, ValueType(killer) };
	if (key == "weapon") return { true, ValueType(weapon) };
	if (key == "part") return { true, ValueType(part) };
	if (key == "damage") return { true, ValueType(damage) };
	if (key == "visible") return { true, ValueType(visible) };
	if (key == "location") return { true, ValueType(location) };
	return { false, ValueType() };
}

void CitizenDeceaseEvent::SetName(std::string name) {
	this->name = move(name);
}

const std::string& CitizenDeceaseEvent::GetName() const {
	return name;
}

void CitizenDeceaseEvent::SetReason(std::string reason) {
	this->reason = move(reason);
}

const std::string& CitizenDeceaseEvent::GetReason() const {
	return reason;
}

void CitizenDeceaseEvent::SetKiller(std::string killer) {
	this->killer = move(killer);
}

const std::string& CitizenDeceaseEvent::GetKiller() const {
	return killer;
}

void CitizenDeceaseEvent::SetWeapon(std::string weapon) {
	this->weapon = move(weapon);
}

const std::string& CitizenDeceaseEvent::GetWeapon() const {
	return weapon;
}

void CitizenDeceaseEvent::SetPart(std::string part) {
	this->part = move(part);
}

const std::string& CitizenDeceaseEvent::GetPart() const {
	return part;
}

void CitizenDeceaseEvent::SetDamage(double damage) {
	this->damage = damage;
}

double CitizenDeceaseEvent::GetDamage() const {
	return damage;
}

void CitizenDeceaseEvent::SetVisible(bool visible) {
	this->visible = visible;
}

bool CitizenDeceaseEvent::GetVisible() const {
	return visible;
}

void CitizenDeceaseEvent::SetLocation(std::string location) {
	this->location = move(location);
}

const std::string& CitizenDeceaseEvent::GetLocation() const {
	return location;
}

PlayerInjuredEvent::PlayerInjuredEvent(std::string wound) :
	wound(move(wound)) {

}

PlayerInjuredEvent::~PlayerInjuredEvent() {

}

const string& PlayerInjuredEvent::GetType() const {
	static const string type = "player_injured";
	return type;
}

void PlayerInjuredEvent::SetWound(std::string wound) {
	this->wound = move(wound);
}

const std::string& PlayerInjuredEvent::GetWound() const {
	return wound;
}

PlayerCuredEvent::PlayerCuredEvent(std::string wound) :
	wound(move(wound)) {

}

PlayerCuredEvent::~PlayerCuredEvent() {

}

const string& PlayerCuredEvent::GetType() const {
	static const string type = "player_cured";
	return type;
}

void PlayerCuredEvent::SetWound(std::string wound) {
	this->wound = move(wound);
}

const std::string& PlayerCuredEvent::GetWound() const {
	return wound;
}

PlayerIllEvent::PlayerIllEvent(std::string illness) :
	illness(move(illness)) {

}

PlayerIllEvent::~PlayerIllEvent() {

}

const string& PlayerIllEvent::GetType() const {
	static const string type = "player_ill";
	return type;
}

void PlayerIllEvent::SetIllness(std::string illness) {
	this->illness = move(illness);
}

const std::string& PlayerIllEvent::GetIllness() const {
	return illness;
}

PlayerRecoverEvent::PlayerRecoverEvent(std::string illness) :
	illness(move(illness)) {

}

PlayerRecoverEvent::~PlayerRecoverEvent() {

}

const string& PlayerRecoverEvent::GetType() const {
	static const string type = "player_recover";
	return type;
}

void PlayerRecoverEvent::SetIllness(std::string illness) {
	this->illness = move(illness);
}

const std::string& PlayerRecoverEvent::GetIllness() const {
	return illness;
}

PlayerRestEvent::PlayerRestEvent(std::string minute) :
	minute(move(minute)) {

}

PlayerRestEvent::~PlayerRestEvent() {

}

const string& PlayerRestEvent::GetType() const {
	static const string type = "player_rest";
	return type;
}

void PlayerRestEvent::SetMinute(std::string minute) {
	this->minute = move(minute);
}

const std::string& PlayerRestEvent::GetMinute() const {
	return minute;
}

PlayerSleepEvent::PlayerSleepEvent(std::string hour) :
	hour(move(hour)) {

}

PlayerSleepEvent::~PlayerSleepEvent() {

}

const string& PlayerSleepEvent::GetType() const {
	static const string type = "player_sleep";
	return type;
}

void PlayerSleepEvent::SetHour(std::string hour) {
	this->hour = move(hour);
}

const std::string& PlayerSleepEvent::GetHour() const {
	return hour;
}

WantedChangeEvent::WantedChangeEvent(std::string reason, std::string level) :
	reason(move(reason)), level(move(level)) {

}

WantedChangeEvent::~WantedChangeEvent() {

}

const string& WantedChangeEvent::GetType() const {
	static const string type = "wanted_change";
	return type;
}

void WantedChangeEvent::SetReason(std::string reason) {
	this->reason = move(reason);
}

const std::string& WantedChangeEvent::GetReason() const {
	return reason;
}

void WantedChangeEvent::SetLevel(std::string level) {
	this->level = move(level);
}

const std::string& WantedChangeEvent::GetLevel() const {
	return level;
}

PlayerArrestedEvent::PlayerArrestedEvent(std::string reason) :
	reason(move(reason)) {

}

PlayerArrestedEvent::~PlayerArrestedEvent() {

}

const string& PlayerArrestedEvent::GetType() const {
	static const string type = "player_arrested";
	return type;
}

void PlayerArrestedEvent::SetReason(std::string reason) {
	this->reason = move(reason);
}

const std::string& PlayerArrestedEvent::GetReason() const {
	return reason;
}

PlayerReleasedEvent::PlayerReleasedEvent(std::string reason) :
	reason(move(reason)) {

}

PlayerReleasedEvent::~PlayerReleasedEvent() {

}

const string& PlayerReleasedEvent::GetType() const {
	static const string type = "player_released";
	return type;
}

void PlayerReleasedEvent::SetReason(std::string reason) {
	this->reason = move(reason);
}

const std::string& PlayerReleasedEvent::GetReason() const {
	return reason;
}

WeatherChangeEvent::WeatherChangeEvent(std::string weather) :
	weather(move(weather)) {

}

WeatherChangeEvent::~WeatherChangeEvent() {

}

const string& WeatherChangeEvent::GetType() const {
	static const string type = "weather_change";
	return type;
}

bool WeatherChangeEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<WeatherChangeEvent*>(e);
	if (!weather.empty() && ToString(EvaluateExpression(weather, context)) != other->weather) return false;
	return true;
}

pair<bool, ValueType> WeatherChangeEvent::GetLocalValue(const string& key) const {
	if (key == "weather") return { true, ValueType(weather) };
	return { false, ValueType() };
}

void WeatherChangeEvent::SetWeather(std::string weather) {
	this->weather = move(weather);
}

const std::string& WeatherChangeEvent::GetWeather() const {
	return weather;
}

UseAssetEvent::UseAssetEvent(std::string asset, std::string type, std::string user) :
	asset(move(asset)), type(move(type)), user(move(user)) {

}

UseAssetEvent::~UseAssetEvent() {

}

const string& UseAssetEvent::GetType() const {
	static const string type = "use_asset";
	return type;
}

void UseAssetEvent::SetAsset(std::string asset) {
	this->asset = move(asset);
}

const std::string& UseAssetEvent::GetAsset() const {
	return asset;
}

pair<bool, ValueType> UseAssetEvent::GetLocalValue(const string& key) const {
	if (key == "asset") return { true, ValueType(asset) };
	if (key == "type") return { true, ValueType(type) };
	if (key == "user") return { true, ValueType(user) };
	return { false, ValueType() };
}

void UseAssetEvent::SetAssetType(std::string type) {
	this->type = move(type);
}

const std::string& UseAssetEvent::GetAssetType() const {
	return type;
}

void UseAssetEvent::SetUser(std::string user) {
	this->user = move(user);
}

const std::string& UseAssetEvent::GetUser() const {
	return user;
}

EnterStopEvent::EnterStopEvent(std::string stop) :
	stop(move(stop)) {

}

EnterStopEvent::~EnterStopEvent() {

}

const string& EnterStopEvent::GetType() const {
	static const string type = "enter_stop";
	return type;
}

bool EnterStopEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;
	return MatchLocationField(stop, static_cast<EnterStopEvent*>(e)->stop, context);
}

pair<bool, ValueType> EnterStopEvent::GetLocalValue(const string& key) const {
	if (key == "stop") return { true, ValueType(stop) };
	return { false, ValueType() };
}

void EnterStopEvent::SetStop(std::string stop) {
	this->stop = move(stop);
}

const std::string& EnterStopEvent::GetStop() const {
	return stop;
}

LeaveStopEvent::LeaveStopEvent(std::string stop) :
	stop(move(stop)) {

}

LeaveStopEvent::~LeaveStopEvent() {

}

const string& LeaveStopEvent::GetType() const {
	static const string type = "leave_stop";
	return type;
}

bool LeaveStopEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;
	return MatchLocationField(stop, static_cast<LeaveStopEvent*>(e)->stop, context);
}

pair<bool, ValueType> LeaveStopEvent::GetLocalValue(const string& key) const {
	if (key == "stop") return { true, ValueType(stop) };
	return { false, ValueType() };
}

void LeaveStopEvent::SetStop(std::string stop) {
	this->stop = move(stop);
}

const std::string& LeaveStopEvent::GetStop() const {
	return stop;
}

EnterVehicleEvent::EnterVehicleEvent(std::string vehicle, std::string mode) :
	vehicle(move(vehicle)), mode(move(mode)) {

}

EnterVehicleEvent::~EnterVehicleEvent() {

}

const string& EnterVehicleEvent::GetType() const {
	static const string type = "enter_vehicle";
	return type;
}

bool EnterVehicleEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<EnterVehicleEvent*>(e);
	if (!MatchLocationField(vehicle, other->vehicle, context)) return false;
	if (!MatchLocationField(mode, other->mode, context)) return false;
	return true;
}

pair<bool, ValueType> EnterVehicleEvent::GetLocalValue(const string& key) const {
	if (key == "vehicle") return { true, ValueType(vehicle) };
	if (key == "mode") return { true, ValueType(mode) };
	return { false, ValueType() };
}

void EnterVehicleEvent::SetVehicle(std::string vehicle) {
	this->vehicle = move(vehicle);
}

const std::string& EnterVehicleEvent::GetVehicle() const {
	return vehicle;
}

void EnterVehicleEvent::SetMode(std::string mode) {
	this->mode = move(mode);
}

const std::string& EnterVehicleEvent::GetMode() const {
	return mode;
}

LeaveVehicleEvent::LeaveVehicleEvent(std::string vehicle, std::string mode) :
	vehicle(move(vehicle)), mode(move(mode)) {

}

LeaveVehicleEvent::~LeaveVehicleEvent() {

}

const string& LeaveVehicleEvent::GetType() const {
	static const string type = "leave_vehicle";
	return type;
}

bool LeaveVehicleEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<LeaveVehicleEvent*>(e);
	if (!MatchLocationField(vehicle, other->vehicle, context)) return false;
	if (!MatchLocationField(mode, other->mode, context)) return false;
	return true;
}

pair<bool, ValueType> LeaveVehicleEvent::GetLocalValue(const string& key) const {
	if (key == "vehicle") return { true, ValueType(vehicle) };
	if (key == "mode") return { true, ValueType(mode) };
	return { false, ValueType() };
}

void LeaveVehicleEvent::SetVehicle(std::string vehicle) {
	this->vehicle = move(vehicle);
}

const std::string& LeaveVehicleEvent::GetVehicle() const {
	return vehicle;
}

void LeaveVehicleEvent::SetMode(std::string mode) {
	this->mode = move(mode);
}

const std::string& LeaveVehicleEvent::GetMode() const {
	return mode;
}

GetAssetEvent::GetAssetEvent(std::string type, std::string result) :
	type(move(type)), result(move(result)) {

}

GetAssetEvent::~GetAssetEvent() {

}

const string& GetAssetEvent::GetType() const {
	static const string type = "get_asset";
	return type;
}

bool GetAssetEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<GetAssetEvent*>(e);
	if (!MatchLocationField(type, other->type, context)) return false;
	if (!MatchLocationField(result, other->result, context)) return false;
	return true;
}

pair<bool, ValueType> GetAssetEvent::GetLocalValue(const string& key) const {
	if (key == "type") return { true, ValueType(type) };
	if (key == "result") return { true, ValueType(result) };
	return { false, ValueType() };
}

void GetAssetEvent::SetAssetType(std::string type) {
	this->type = move(type);
}

const std::string& GetAssetEvent::GetAssetType() const {
	return type;
}

void GetAssetEvent::SetResult(std::string result) {
	this->result = move(result);
}

const std::string& GetAssetEvent::GetResult() const {
	return result;
}

LoseAssetEvent::LoseAssetEvent(std::string type, std::string result) :
	type(move(type)), result(move(result)) {

}

LoseAssetEvent::~LoseAssetEvent() {

}

const string& LoseAssetEvent::GetType() const {
	static const string type = "lose_asset";
	return type;
}

bool LoseAssetEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<LoseAssetEvent*>(e);
	if (!MatchLocationField(type, other->type, context)) return false;
	if (!MatchLocationField(result, other->result, context)) return false;
	return true;
}

pair<bool, ValueType> LoseAssetEvent::GetLocalValue(const string& key) const {
	if (key == "type") return { true, ValueType(type) };
	if (key == "result") return { true, ValueType(result) };
	return { false, ValueType() };
}

void LoseAssetEvent::SetAssetType(std::string type) {
	this->type = move(type);
}

const std::string& LoseAssetEvent::GetAssetType() const {
	return type;
}

void LoseAssetEvent::SetResult(std::string result) {
	this->result = move(result);
}

const std::string& LoseAssetEvent::GetResult() const {
	return result;
}

ThrowAssetEvent::ThrowAssetEvent(std::string type, std::string thrower) :
	type(move(type)), thrower(move(thrower)) {

}

ThrowAssetEvent::~ThrowAssetEvent() {

}

const string& ThrowAssetEvent::GetType() const {
	static const string type = "throw_asset";
	return type;
}

bool ThrowAssetEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<ThrowAssetEvent*>(e);
	return MatchLocationField(type, other->type, context);
}

pair<bool, ValueType> ThrowAssetEvent::GetLocalValue(const string& key) const {
	if (key == "type") return { true, ValueType(type) };
	if (key == "thrower") return { true, ValueType(thrower) };
	return { false, ValueType() };
}

void ThrowAssetEvent::SetThrower(std::string thrower) {
	this->thrower = move(thrower);
}

const std::string& ThrowAssetEvent::GetThrower() const {
	return thrower;
}

void ThrowAssetEvent::SetAssetType(std::string type) {
	this->type = move(type);
}

const std::string& ThrowAssetEvent::GetAssetType() const {
	return type;
}

WeaponFireEvent::WeaponFireEvent(std::string weapon) :
	weapon(move(weapon)) {

}

WeaponFireEvent::~WeaponFireEvent() {

}

const string& WeaponFireEvent::GetType() const {
	static const string type = "weapon_fire";
	return type;
}

bool WeaponFireEvent::Match(Event* e, const ScriptContext& context) {
	if (!Event::Match(e, context)) return false;

	auto* other = static_cast<WeaponFireEvent*>(e);
	return MatchLocationField(weapon, other->weapon, context);
}

pair<bool, ValueType> WeaponFireEvent::GetLocalValue(const string& key) const {
	if (key == "weapon") return { true, ValueType(weapon) };
	auto it = fields.find(key);
	if (it == fields.end()) return { false, ValueType() };
	return { true, it->second };
}

void WeaponFireEvent::SetWeapon(std::string weapon) {
	this->weapon = move(weapon);
}

const std::string& WeaponFireEvent::GetWeapon() const {
	return weapon;
}

void WeaponFireEvent::SetField(const std::string& name, ValueType value) {
	fields[name] = move(value);
}

void WeaponFireEvent::SetVec3(const std::string& name, double x, double y, double z) {
	StoreVec3(fields, name, x, y, z);
}

ProjectileHitEvent::ProjectileHitEvent() {

}

ProjectileHitEvent::~ProjectileHitEvent() {

}

const string& ProjectileHitEvent::GetType() const {
	static const string type = "projectile_hit";
	return type;
}

bool ProjectileHitEvent::Match(Event* e, const ScriptContext& context) {
	return Event::Match(e, context);
}

pair<bool, ValueType> ProjectileHitEvent::GetLocalValue(const string& key) const {
	auto it = fields.find(key);
	if (it == fields.end()) return { false, ValueType() };
	return { true, it->second };
}

void ProjectileHitEvent::SetField(const std::string& name, ValueType value) {
	fields[name] = move(value);
}

void ProjectileHitEvent::SetVec3(const std::string& name, double x, double y, double z) {
	StoreVec3(fields, name, x, y, z);
}
