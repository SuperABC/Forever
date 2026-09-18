#include "event.h"


using namespace std;

Event::Event() :
	condition() {

}

Event::~Event() {

}

bool Event::Match(Event* e, const ScriptContext& context) {
	if (!e) return false;
	return GetType() == e->GetType();
}

const Expression& Event::GetCondition() const {
	return condition;
}

void Event::SetCondition(const Expression& condition) {
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

GlobalMessageEvent::GlobalMessageEvent(Expression message) :
	message(move(message)) {

}

GlobalMessageEvent::~GlobalMessageEvent() {

}

const string& GlobalMessageEvent::GetType() const {
	static const string type = "global_message";
	return type;
}

void GlobalMessageEvent::SetMessage(Expression message) {
	this->message = move(message);
}

const Expression& GlobalMessageEvent::GetMessage() const {
	return message;
}

OptionDialogEvent::OptionDialogEvent(Expression name, Expression option) :
	name(move(name)), option(move(option)) {

}

OptionDialogEvent::~OptionDialogEvent() {

}

const string& OptionDialogEvent::GetType() const {
	static const string type = "option_dialog";
	return type;
}

void OptionDialogEvent::SetName(Expression name) {
	this->name = move(name);
}

const Expression& OptionDialogEvent::GetName() const {
	return name;
}

void OptionDialogEvent::SetOption(Expression option) {
	this->option = move(option);
}

const Expression& OptionDialogEvent::GetOption() const {
	return option;
}

GlobalDialogEvent::GlobalDialogEvent(Expression name, Expression option) :
	name(move(name)), option(move(option)) {

}

GlobalDialogEvent::~GlobalDialogEvent() {

}

const string& GlobalDialogEvent::GetType() const {
	static const string type = "global_dialog";
	return type;
}

void GlobalDialogEvent::SetName(Expression name) {
	this->name = move(name);
}

const Expression& GlobalDialogEvent::GetName() const {
	return name;
}

void GlobalDialogEvent::SetOption(Expression option) {
	this->option = move(option);
}

const Expression& GlobalDialogEvent::GetOption() const {
	return option;
}

SpeakingFinishEvent::SpeakingFinishEvent(Expression label) :
	label(move(label)) {

}

SpeakingFinishEvent::~SpeakingFinishEvent() {

}

const string& SpeakingFinishEvent::GetType() const {
	static const string type = "speaking_finish";
	return type;
}

void SpeakingFinishEvent::SetLabel(Expression label) {
	this->label = move(label);
}

const Expression& SpeakingFinishEvent::GetLabel() const {
	return label;
}

EnterZoneEvent::EnterZoneEvent(Expression zone) :
	zone(move(zone)) {

}

EnterZoneEvent::~EnterZoneEvent() {

}

const string& EnterZoneEvent::GetType() const {
	static const string type = "enter_zone";
	return type;
}

void EnterZoneEvent::SetZone(Expression zone) {
	this->zone = move(zone);
}

const Expression& EnterZoneEvent::GetZone() const {
	return zone;
}

LeaveZoneEvent::LeaveZoneEvent(Expression zone) :
	zone(move(zone)) {

}

LeaveZoneEvent::~LeaveZoneEvent() {

}

const string& LeaveZoneEvent::GetType() const {
	static const string type = "leave_zone";
	return type;
}

void LeaveZoneEvent::SetZone(Expression zone) {
	this->zone = move(zone);
}

const Expression& LeaveZoneEvent::GetZone() const {
	return zone;
}

EnterBuildingEvent::EnterBuildingEvent(Expression zone, Expression building) :
	zone(move(zone)), building(move(building)) {

}

EnterBuildingEvent::~EnterBuildingEvent() {

}

const string& EnterBuildingEvent::GetType() const {
	static const string type = "enter_building";
	return type;
}

void EnterBuildingEvent::SetZone(Expression zone) {
	this->zone = move(zone);
}

const Expression& EnterBuildingEvent::GetZone() const {
	return zone;
}

void EnterBuildingEvent::SetBuilding(Expression building) {
	this->building = move(building);
}

const Expression& EnterBuildingEvent::GetBuilding() const {
	return building;
}

LeaveBuildingEvent::LeaveBuildingEvent(Expression zone, Expression building) :
	zone(move(zone)), building(move(building)) {

}

LeaveBuildingEvent::~LeaveBuildingEvent() {

}

const string& LeaveBuildingEvent::GetType() const {
	static const string type = "leave_building";
	return type;
}

void LeaveBuildingEvent::SetZone(Expression zone) {
	this->zone = move(zone);
}

const Expression& LeaveBuildingEvent::GetZone() const {
	return zone;
}

void LeaveBuildingEvent::SetBuilding(Expression building) {
	this->building = move(building);
}

const Expression& LeaveBuildingEvent::GetBuilding() const {
	return building;
}

EnterRoomEvent::EnterRoomEvent(Expression zone, Expression building, Expression room) :
	zone(move(zone)), building(move(building)), room(move(room)) {

}

EnterRoomEvent::~EnterRoomEvent() {

}

const string& EnterRoomEvent::GetType() const {
	static const string type = "enter_room";
	return type;
}

void EnterRoomEvent::SetZone(Expression zone) {
	this->zone = move(zone);
}

const Expression& EnterRoomEvent::GetZone() const {
	return zone;
}

void EnterRoomEvent::SetBuilding(Expression building) {
	this->building = move(building);
}

const Expression& EnterRoomEvent::GetBuilding() const {
	return building;
}

void EnterRoomEvent::SetRoom(Expression room) {
	this->room = move(room);
}

const Expression& EnterRoomEvent::GetRoom() const {
	return room;
}

LeaveRoomEvent::LeaveRoomEvent(Expression zone, Expression building, Expression room) :
	zone(move(zone)), building(move(building)), room(move(room)) {

}

LeaveRoomEvent::~LeaveRoomEvent() {

}

const string& LeaveRoomEvent::GetType() const {
	static const string type = "leave_room";
	return type;
}

void LeaveRoomEvent::SetZone(Expression zone) {
	this->zone = move(zone);
}

const Expression& LeaveRoomEvent::GetZone() const {
	return zone;
}

void LeaveRoomEvent::SetBuilding(Expression building) {
	this->building = move(building);
}

const Expression& LeaveRoomEvent::GetBuilding() const {
	return building;
}

void LeaveRoomEvent::SetRoom(Expression room) {
	this->room = move(room);
}

const Expression& LeaveRoomEvent::GetRoom() const {
	return room;
}

PuzzleResultEvent::PuzzleResultEvent(Expression result) :
	result(move(result)) {

}

PuzzleResultEvent::~PuzzleResultEvent() {

}

const string& PuzzleResultEvent::GetType() const {
	static const string type = "puzzle_result";
	return type;
}

void PuzzleResultEvent::SetResult(Expression result) {
	this->result = move(result);
}

const Expression& PuzzleResultEvent::GetResult() const {
	return result;
}

TransactionResultEvent::TransactionResultEvent(Expression result, Expression name) :
	result(move(result)), name(move(name)) {

}

TransactionResultEvent::~TransactionResultEvent() {

}

const string& TransactionResultEvent::GetType() const {
	static const string type = "transaction_result";
	return type;
}

void TransactionResultEvent::SetResult(Expression result) {
	this->result = move(result);
}

const Expression& TransactionResultEvent::GetResult() const {
	return result;
}

void TransactionResultEvent::SetName(Expression name) {
	this->name = move(name);
}

const Expression& TransactionResultEvent::GetName() const {
	return name;
}

ObjectResultEvent::ObjectResultEvent(Expression action, Expression object, Expression result, Expression num) :
	action(move(action)), object(move(object)), result(move(result)), num(move(num)) {

}

ObjectResultEvent::~ObjectResultEvent() {

}

const string& ObjectResultEvent::GetType() const {
	static const string type = "object_result";
	return type;
}

void ObjectResultEvent::SetAction(Expression action) {
	this->action = move(action);
}

const Expression& ObjectResultEvent::GetAction() const {
	return action;
}

void ObjectResultEvent::SetObject(Expression object) {
	this->object = move(object);
}

const Expression& ObjectResultEvent::GetObject() const {
	return object;
}

void ObjectResultEvent::SetResult(Expression result) {
	this->result = move(result);
}

const Expression& ObjectResultEvent::GetResult() const {
	return result;
}

void ObjectResultEvent::SetNum(Expression num) {
	this->num = move(num);
}

const Expression& ObjectResultEvent::GetNum() const {
	return num;
}

TimeUpEvent::TimeUpEvent(Expression name) :
	name(move(name)) {

}

TimeUpEvent::~TimeUpEvent() {

}

const string& TimeUpEvent::GetType() const {
	static const string type = "time_up";
	return type;
}

void TimeUpEvent::SetName(Expression name) {
	this->name = move(name);
}

const Expression& TimeUpEvent::GetName() const {
	return name;
}

NpcArriveEvent::NpcArriveEvent(Expression name, Expression address) :
	name(move(name)), address(move(address)) {

}

NpcArriveEvent::~NpcArriveEvent() {

}

const string& NpcArriveEvent::GetType() const {
	static const string type = "npc_arrive";
	return type;
}

void NpcArriveEvent::SetName(Expression name) {
	this->name = move(name);
}

const Expression& NpcArriveEvent::GetName() const {
	return name;
}

void NpcArriveEvent::SetAddress(Expression address) {
	this->address = move(address);
}

const Expression& NpcArriveEvent::GetAddress() const {
	return address;
}

NPCMeetEvent::NPCMeetEvent(Expression npc) :
	npc(move(npc)) {

}

NPCMeetEvent::~NPCMeetEvent() {

}

const string& NPCMeetEvent::GetType() const {
	static const string type = "npc_meet";
	return type;
}

void NPCMeetEvent::SetNPC(Expression npc) {
	this->npc = move(npc);
}

const Expression& NPCMeetEvent::GetNPC() const {
	return npc;
}

CitizenBornEvent::CitizenBornEvent(Expression name) :
	name(move(name)) {

}

CitizenBornEvent::~CitizenBornEvent() {

}

const string& CitizenBornEvent::GetType() const {
	static const string type = "citizen_born";
	return type;
}

void CitizenBornEvent::SetName(Expression name) {
	this->name = move(name);
}

const Expression& CitizenBornEvent::GetName() const {
	return name;
}

CitizenDeceaseEvent::CitizenDeceaseEvent(Expression name, Expression reason) :
	name(move(name)), reason(move(reason)) {

}

CitizenDeceaseEvent::~CitizenDeceaseEvent() {

}

const string& CitizenDeceaseEvent::GetType() const {
	static const string type = "citizen_decease";
	return type;
}

void CitizenDeceaseEvent::SetName(Expression name) {
	this->name = move(name);
}

const Expression& CitizenDeceaseEvent::GetName() const {
	return name;
}

void CitizenDeceaseEvent::SetReason(Expression reason) {
	this->reason = move(reason);
}

const Expression& CitizenDeceaseEvent::GetReason() const {
	return reason;
}

PlayerInjuredEvent::PlayerInjuredEvent(Expression wound) :
	wound(move(wound)) {

}

PlayerInjuredEvent::~PlayerInjuredEvent() {

}

const string& PlayerInjuredEvent::GetType() const {
	static const string type = "player_injured";
	return type;
}

void PlayerInjuredEvent::SetWound(Expression wound) {
	this->wound = move(wound);
}

const Expression& PlayerInjuredEvent::GetWound() const {
	return wound;
}

PlayerCuredEvent::PlayerCuredEvent(Expression wound) :
	wound(move(wound)) {

}

PlayerCuredEvent::~PlayerCuredEvent() {

}

const string& PlayerCuredEvent::GetType() const {
	static const string type = "player_cured";
	return type;
}

void PlayerCuredEvent::SetWound(Expression wound) {
	this->wound = move(wound);
}

const Expression& PlayerCuredEvent::GetWound() const {
	return wound;
}

PlayerIllEvent::PlayerIllEvent(Expression illness) :
	illness(move(illness)) {

}

PlayerIllEvent::~PlayerIllEvent() {

}

const string& PlayerIllEvent::GetType() const {
	static const string type = "player_ill";
	return type;
}

void PlayerIllEvent::SetIllness(Expression illness) {
	this->illness = move(illness);
}

const Expression& PlayerIllEvent::GetIllness() const {
	return illness;
}

PlayerRecoverEvent::PlayerRecoverEvent(Expression illness) :
	illness(move(illness)) {

}

PlayerRecoverEvent::~PlayerRecoverEvent() {

}

const string& PlayerRecoverEvent::GetType() const {
	static const string type = "player_recover";
	return type;
}

void PlayerRecoverEvent::SetIllness(Expression illness) {
	this->illness = move(illness);
}

const Expression& PlayerRecoverEvent::GetIllness() const {
	return illness;
}

PlayerRestEvent::PlayerRestEvent(Expression minute) :
	minute(move(minute)) {

}

PlayerRestEvent::~PlayerRestEvent() {

}

const string& PlayerRestEvent::GetType() const {
	static const string type = "player_rest";
	return type;
}

void PlayerRestEvent::SetMinute(Expression minute) {
	this->minute = move(minute);
}

const Expression& PlayerRestEvent::GetMinute() const {
	return minute;
}

PlayerSleepEvent::PlayerSleepEvent(Expression hour) :
	hour(move(hour)) {

}

PlayerSleepEvent::~PlayerSleepEvent() {

}

const string& PlayerSleepEvent::GetType() const {
	static const string type = "player_sleep";
	return type;
}

void PlayerSleepEvent::SetHour(Expression hour) {
	this->hour = move(hour);
}

const Expression& PlayerSleepEvent::GetHour() const {
	return hour;
}

CultivationChangeEvent::CultivationChangeEvent(Expression method, Expression level) :
	method(move(method)), level(move(level)) {

}

CultivationChangeEvent::~CultivationChangeEvent() {

}

const string& CultivationChangeEvent::GetType() const {
	static const string type = "cultivation_change";
	return type;
}

void CultivationChangeEvent::SetMethod(Expression method) {
	this->method = move(method);
}

const Expression& CultivationChangeEvent::GetMethod() const {
	return method;
}

void CultivationChangeEvent::SetLevel(Expression level) {
	this->level = move(level);
}

const Expression& CultivationChangeEvent::GetLevel() const {
	return level;
}

WantedChangeEvent::WantedChangeEvent(Expression reason, Expression level) :
	reason(move(reason)), level(move(level)) {

}

WantedChangeEvent::~WantedChangeEvent() {

}

const string& WantedChangeEvent::GetType() const {
	static const string type = "wanted_change";
	return type;
}

void WantedChangeEvent::SetReason(Expression reason) {
	this->reason = move(reason);
}

const Expression& WantedChangeEvent::GetReason() const {
	return reason;
}

void WantedChangeEvent::SetLevel(Expression level) {
	this->level = move(level);
}

const Expression& WantedChangeEvent::GetLevel() const {
	return level;
}

PlayerArrestedEvent::PlayerArrestedEvent(Expression reason) :
	reason(move(reason)) {

}

PlayerArrestedEvent::~PlayerArrestedEvent() {

}

const string& PlayerArrestedEvent::GetType() const {
	static const string type = "player_arrested";
	return type;
}

void PlayerArrestedEvent::SetReason(Expression reason) {
	this->reason = move(reason);
}

const Expression& PlayerArrestedEvent::GetReason() const {
	return reason;
}

PlayerReleasedEvent::PlayerReleasedEvent(Expression reason) :
	reason(move(reason)) {

}

PlayerReleasedEvent::~PlayerReleasedEvent() {

}

const string& PlayerReleasedEvent::GetType() const {
	static const string type = "player_released";
	return type;
}

void PlayerReleasedEvent::SetReason(Expression reason) {
	this->reason = move(reason);
}

const Expression& PlayerReleasedEvent::GetReason() const {
	return reason;
}

WeatherChangeEvent::WeatherChangeEvent(Expression weather) :
	weather(move(weather)) {

}

WeatherChangeEvent::~WeatherChangeEvent() {

}

const string& WeatherChangeEvent::GetType() const {
	static const string type = "weather_change";
	return type;
}

void WeatherChangeEvent::SetWeather(Expression weather) {
	this->weather = move(weather);
}

const Expression& WeatherChangeEvent::GetWeather() const {
	return weather;
}

PolicyChangeEvent::PolicyChangeEvent(Expression policy, Expression status) :
	policy(move(policy)), status(move(status)) {

}

PolicyChangeEvent::~PolicyChangeEvent() {

}

const string& PolicyChangeEvent::GetType() const {
	static const string type = "policy_change";
	return type;
}

void PolicyChangeEvent::SetPolicy(Expression policy) {
	this->policy = move(policy);
}

const Expression& PolicyChangeEvent::GetPolicy() const {
	return policy;
}

void PolicyChangeEvent::SetStatus(Expression status) {
	this->status = move(status);
}

const Expression& PolicyChangeEvent::GetStatus() const {
	return status;
}

UseAssetEvent::UseAssetEvent(Expression asset) :
	asset(move(asset)) {

}

UseAssetEvent::~UseAssetEvent() {

}

const string& UseAssetEvent::GetType() const {
	static const string type = "use_asset";
	return type;
}

void UseAssetEvent::SetAsset(Expression asset) {
	this->asset = move(asset);
}

const Expression& UseAssetEvent::GetAsset() const {
	return asset;
}
