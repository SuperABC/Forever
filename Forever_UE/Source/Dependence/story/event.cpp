#include "event.h"


using namespace std;

namespace {
	// 位置类事件(进出园区/建筑/房间)的字段匹配：触发条件里的字段是表达式，留空表示不限。
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

PuzzleResultEvent::PuzzleResultEvent(std::string result) :
	result(move(result)) {

}

PuzzleResultEvent::~PuzzleResultEvent() {

}

const string& PuzzleResultEvent::GetType() const {
	static const string type = "puzzle_result";
	return type;
}

void PuzzleResultEvent::SetResult(std::string result) {
	this->result = move(result);
}

const std::string& PuzzleResultEvent::GetResult() const {
	return result;
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

ObjectResultEvent::ObjectResultEvent(std::string action, std::string object, std::string result, std::string num) :
	action(move(action)), object(move(object)), result(move(result)), num(move(num)) {

}

ObjectResultEvent::~ObjectResultEvent() {

}

const string& ObjectResultEvent::GetType() const {
	static const string type = "object_result";
	return type;
}

void ObjectResultEvent::SetAction(std::string action) {
	this->action = move(action);
}

const std::string& ObjectResultEvent::GetAction() const {
	return action;
}

void ObjectResultEvent::SetObject(std::string object) {
	this->object = move(object);
}

const std::string& ObjectResultEvent::GetObject() const {
	return object;
}

void ObjectResultEvent::SetResult(std::string result) {
	this->result = move(result);
}

const std::string& ObjectResultEvent::GetResult() const {
	return result;
}

void ObjectResultEvent::SetNum(std::string num) {
	this->num = move(num);
}

const std::string& ObjectResultEvent::GetNum() const {
	return num;
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
	return true;
}

pair<bool, ValueType> CitizenDeceaseEvent::GetLocalValue(const string& key) const {
	if (key == "name") return { true, ValueType(name) };
	if (key == "reason") return { true, ValueType(reason) };
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

CultivationChangeEvent::CultivationChangeEvent(std::string method, std::string level) :
	method(move(method)), level(move(level)) {

}

CultivationChangeEvent::~CultivationChangeEvent() {

}

const string& CultivationChangeEvent::GetType() const {
	static const string type = "cultivation_change";
	return type;
}

void CultivationChangeEvent::SetMethod(std::string method) {
	this->method = move(method);
}

const std::string& CultivationChangeEvent::GetMethod() const {
	return method;
}

void CultivationChangeEvent::SetLevel(std::string level) {
	this->level = move(level);
}

const std::string& CultivationChangeEvent::GetLevel() const {
	return level;
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

void WeatherChangeEvent::SetWeather(std::string weather) {
	this->weather = move(weather);
}

const std::string& WeatherChangeEvent::GetWeather() const {
	return weather;
}

PolicyChangeEvent::PolicyChangeEvent(std::string policy, std::string status) :
	policy(move(policy)), status(move(status)) {

}

PolicyChangeEvent::~PolicyChangeEvent() {

}

const string& PolicyChangeEvent::GetType() const {
	static const string type = "policy_change";
	return type;
}

void PolicyChangeEvent::SetPolicy(std::string policy) {
	this->policy = move(policy);
}

const std::string& PolicyChangeEvent::GetPolicy() const {
	return policy;
}

void PolicyChangeEvent::SetStatus(std::string status) {
	this->status = move(status);
}

const std::string& PolicyChangeEvent::GetStatus() const {
	return status;
}

UseAssetEvent::UseAssetEvent(std::string asset) :
	asset(move(asset)) {

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
