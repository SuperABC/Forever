#include "event.h"


using namespace std;

Event::Event() :
	condition() {

}

Event::~Event() {

}

const Condition& Event::GetCondition() const {
	return condition;
}

void Event::SetCondition(const Condition& condition) {
	this->condition = condition;
}

GameStartEvent::GameStartEvent() {

}

GameStartEvent::~GameStartEvent() {

}

const string& GameStartEvent::GetType() const {
	static const string type = "game_start";
	return type;
}

bool GameStartEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;
	return true;
}

GlobalMessageEvent::GlobalMessageEvent(string message) :
	message(message) {

}

GlobalMessageEvent::~GlobalMessageEvent() {

}

const string& GlobalMessageEvent::GetType() const {
	static const string type = "global_message";
	return type;
}

bool GlobalMessageEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<GlobalMessageEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (message.size() > 0 && other->message.size() > 0) {
		Condition condition;
		condition.ParseCondition(message);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->message);
	}

	return result;
}

void GlobalMessageEvent::SetMessage(string message) {
	this->message = message;
}

string GlobalMessageEvent::GetMessage() const {
	return message;
}

OptionDialogEvent::OptionDialogEvent(string name, string option) :
	id(-1), name(name), option(option) {

}

OptionDialogEvent::OptionDialogEvent(int id, string option) :
	id(id), name(""), option(option) {

}

OptionDialogEvent::~OptionDialogEvent() {

}

const string& OptionDialogEvent::GetType() const {
	static const string type = "option_dialog";
	return type;
}

bool OptionDialogEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<OptionDialogEvent*>(e);
	if (!other) return false;

	if (id == -1 && name == "" ||
		other->id == -1 && other->name == "") {
		return option == other->option;
	}

	bool result = true;
	if (name.size() > 0 && other->name.size() > 0) {
		Condition condition;
		condition.ParseCondition(name);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->name);
	}
	if (option.size() > 0 && other->option.size() > 0) {
		Condition condition;
		condition.ParseCondition(option);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->option);
	}

	return result;
}

void OptionDialogEvent::SetId(int id) {
	this->id = id;
}

int OptionDialogEvent::GetId() const {
	return id;
}

void OptionDialogEvent::SetName(string name) {
	this->name = name;
}

string OptionDialogEvent::GetName() const {
	return name;
}

void OptionDialogEvent::SetOption(string option) {
	this->option = option;
}

string OptionDialogEvent::GetOption() const {
	return option;
}

GlobalDialogEvent::GlobalDialogEvent(string name, string option) :
	name(name), option(option) {

}

GlobalDialogEvent::~GlobalDialogEvent() {

}

const string& GlobalDialogEvent::GetType() const {
	static const string type = "global_dialog";
	return type;
}

bool GlobalDialogEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<GlobalDialogEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (name.size() > 0 && other->name.size() > 0) {
		Condition condition;
		condition.ParseCondition(name);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->name);
	}
	if (option.size() > 0 && other->option.size() > 0) {
		Condition condition;
		condition.ParseCondition(option);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->option);
	}

	return result;
}

void GlobalDialogEvent::SetName(string name) {
	this->name = name;
}

string GlobalDialogEvent::GetName() const {
	return name;
}

void GlobalDialogEvent::SetOption(string option) {
	this->option = option;
}

string GlobalDialogEvent::GetOption() const {
	return option;
}

SpeakingFinishEvent::SpeakingFinishEvent(string label) :
	label(label) {

}

SpeakingFinishEvent::~SpeakingFinishEvent() {

}

const string& SpeakingFinishEvent::GetType() const {
	static const string type = "speaking_finish";
	return type;
}

bool SpeakingFinishEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;
	auto other = dynamic_cast<SpeakingFinishEvent*>(e);
	if (!other) return false;
	bool result = true;
	if (label.size() > 0 && other->label.size() > 0) {
		Condition condition;
		condition.ParseCondition(label);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->label);
	}
	else {
		result = false;
	}
	return result;
}

void SpeakingFinishEvent::SetLabel(string label) {
	this->label = label;
}

string SpeakingFinishEvent::GetLabel() const {
	return label;
}

EnterZoneEvent::EnterZoneEvent(string zone) :
	zone(zone) {

}

EnterZoneEvent::~EnterZoneEvent() {

}

const string& EnterZoneEvent::GetType() const {
	static const string type = "enter_zone";
	return type;
}

bool EnterZoneEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<EnterZoneEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (zone.size() > 0 && other->zone.size() > 0) {
		Condition condition;
		condition.ParseCondition(zone);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->zone);
	}

	return result;
}

void EnterZoneEvent::SetZone(string zone) {
	this->zone = zone;
}

string EnterZoneEvent::GetZone() const {
	return zone;
}

LeaveZoneEvent::LeaveZoneEvent(string zone) :
	zone(zone) {

}

LeaveZoneEvent::~LeaveZoneEvent() {

}

const string& LeaveZoneEvent::GetType() const {
	static const string type = "leave_zone";
	return type;
}

bool LeaveZoneEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<LeaveZoneEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (zone.size() > 0 && other->zone.size() > 0) {
		Condition condition;
		condition.ParseCondition(zone);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->zone);
	}
	return result;
}

void LeaveZoneEvent::SetZone(string zone) {
	this->zone = zone;
}

string LeaveZoneEvent::GetZone() const {
	return zone;
}

EnterBuildingEvent::EnterBuildingEvent(string zone, string building)
	: zone(zone), building(building) {

}

EnterBuildingEvent::~EnterBuildingEvent() {

}

const string& EnterBuildingEvent::GetType() const {
	static const string type = "enter_building";
	return type;
}

bool EnterBuildingEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<EnterBuildingEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (zone.size() > 0 && other->zone.size() > 0) {
		Condition condition;
		condition.ParseCondition(zone);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->zone);
	}
	if (building.size() > 0 && other->building.size() > 0) {
		Condition condition;
		condition.ParseCondition(building);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->building);
	}
	return result;
}

void EnterBuildingEvent::SetZone(string zone) {
	this->zone = zone;
}

string EnterBuildingEvent::GetZone() const {
	return zone;
}

void EnterBuildingEvent::SetBuilding(string building) {
	this->building = building;
}

string EnterBuildingEvent::GetBuilding() const {
	return building;
}

LeaveBuildingEvent::LeaveBuildingEvent(string zone, string building)
	: zone(zone), building(building) {

}

LeaveBuildingEvent::~LeaveBuildingEvent() {

}

const string& LeaveBuildingEvent::GetType() const {
	static const string type = "leave_building";
	return type;
}

bool LeaveBuildingEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<LeaveBuildingEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (zone.size() > 0 && other->zone.size() > 0) {
		Condition condition;
		condition.ParseCondition(zone);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->zone);
	}
	if (building.size() > 0 && other->building.size() > 0) {
		Condition condition;
		condition.ParseCondition(building);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->building);
	}
	return result;
}

void LeaveBuildingEvent::SetZone(string zone) {
	this->zone = zone;
}

string LeaveBuildingEvent::GetZone() const {
	return zone;
}

void LeaveBuildingEvent::SetBuilding(string building) {
	this->building = building;
}

string LeaveBuildingEvent::GetBuilding() const {
	return building;
}

EnterRoomEvent::EnterRoomEvent(string zone, string building, string room)
	: zone(zone), building(building), room(room) {

}

EnterRoomEvent::~EnterRoomEvent() {

}

const string& EnterRoomEvent::GetType() const {
	static const string type = "enter_room";
	return type;
}

bool EnterRoomEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<EnterRoomEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (zone.size() > 0 && other->zone.size() > 0) {
		Condition condition;
		condition.ParseCondition(zone);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->zone);
	}
	if (building.size() > 0 && other->building.size() > 0) {
		Condition condition;
		condition.ParseCondition(building);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->building);
	}
	if (room.size() > 0 && other->room.size() > 0) {
		Condition condition;
		condition.ParseCondition(room);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->room);
	}
	return result;
}

void EnterRoomEvent::SetZone(string zone) {
	this->zone = zone;
}

string EnterRoomEvent::GetZone() const {
	return zone;
}

void EnterRoomEvent::SetBuilding(string building) {
	this->building = building;
}

string EnterRoomEvent::GetBuilding() const {
	return building;
}

void EnterRoomEvent::SetRoom(string room) {
	this->room = room;
}

string EnterRoomEvent::GetRoom() const {
	return room;
}

LeaveRoomEvent::LeaveRoomEvent(string zone, string building, string room)
	: zone(zone), building(building), room(room) {

}

LeaveRoomEvent::~LeaveRoomEvent() {

}

const string& LeaveRoomEvent::GetType() const {
	static const string type = "leave_room";
	return type;
}

bool LeaveRoomEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<LeaveRoomEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (zone.size() > 0 && other->zone.size() > 0) {
		Condition condition;
		condition.ParseCondition(zone);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->zone);
	}
	if (building.size() > 0 && other->building.size() > 0) {
		Condition condition;
		condition.ParseCondition(building);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->building);
	}
	if (room.size() > 0 && other->room.size() > 0) {
		Condition condition;
		condition.ParseCondition(room);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->room);
	}
	return result;
}

void LeaveRoomEvent::SetZone(string zone) {
	this->zone = zone;
}

string LeaveRoomEvent::GetZone() const {
	return zone;
}

void LeaveRoomEvent::SetBuilding(string building) {
	this->building = building;
}

string LeaveRoomEvent::GetBuilding() const {
	return building;
}

void LeaveRoomEvent::SetRoom(string room) {
	this->room = room;
}

string LeaveRoomEvent::GetRoom() const {
	return room;
}

PuzzleResultEvent::PuzzleResultEvent(int result)
	: result(result) {
}

PuzzleResultEvent::~PuzzleResultEvent() {
}

const string& PuzzleResultEvent::GetType() const {
	static const string type = "puzzle_result";
	return type;
}

bool PuzzleResultEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<PuzzleResultEvent*>(e);
	if (!other) return false;

	return result == 0 || result == other->result;
}

void PuzzleResultEvent::SetResult(int result) {
	this->result = result;
}

int PuzzleResultEvent::GetResult() const {
	return result;
}

TransactionResultEvent::TransactionResultEvent(bool result, string name) :
	result(result), name(name) {

}

TransactionResultEvent::~TransactionResultEvent() {

}

const string& TransactionResultEvent::GetType() const {
	static const string type = "transaction_result";
	return type;
}

bool TransactionResultEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<TransactionResultEvent*>(e);
	if (!other) return false;

	bool matched = result == other->result;
	if (name.size() > 0) {
		Condition condition;
		condition.ParseCondition(name);
		matched = matched && (ToString(condition.EvaluateValue(getValues)) == other->name);
	}
	else {
		matched = matched && other->name.empty();
	}
	return matched;
}

void TransactionResultEvent::SetResult(bool result) {
	this->result = result;
}

bool TransactionResultEvent::GetResult() const {
	return result;
}

void TransactionResultEvent::SetName(string name) {
	this->name = name;
}

string TransactionResultEvent::GetName() const {
	return name;
}

ObjectResultEvent::ObjectResultEvent(string action, string object, bool result, int num) :
	action(action), object(object), result(result), num(num) {

}

ObjectResultEvent::~ObjectResultEvent() {

}

const string& ObjectResultEvent::GetType() const {
	static const string type = "object_result";
	return type;
}

bool ObjectResultEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<ObjectResultEvent*>(e);
	if (!other) return false;

	if (!action.empty() && action != other->action) return false;

	if (!object.empty() && !other->object.empty()) {
		Condition objectCondition;
		objectCondition.ParseCondition(object);
		if (ToString(objectCondition.EvaluateValue(getValues)) != other->object) return false;
	}

	return true;
}

void ObjectResultEvent::SetAction(string action) {
	this->action = action;
}

string ObjectResultEvent::GetAction() const {
	return action;
}

void ObjectResultEvent::SetObject(string object) {
	this->object = object;
}

string ObjectResultEvent::GetObject() const {
	return object;
}

void ObjectResultEvent::SetResult(bool result) {
	this->result = result;
}

bool ObjectResultEvent::GetResult() const {
	return result;
}

void ObjectResultEvent::SetNum(int num) {
	this->num = num;
}

int ObjectResultEvent::GetNum() const {
	return num;
}

TimeUpEvent::TimeUpEvent(string name) :
	name(name) {

}

TimeUpEvent::~TimeUpEvent() {

}

const string& TimeUpEvent::GetType() const {
	static const string type = "time_up";
	return type;
}

bool TimeUpEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<TimeUpEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (name.size() > 0 && other->name.size() > 0) {
		Condition condition;
		condition.ParseCondition(name);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->name);
	}
	return result;
}

void TimeUpEvent::SetName(string name) {
	this->name = name;
}

string TimeUpEvent::GetName() const {
	return name;
}

NpcArriveEvent::NpcArriveEvent(string name, string address)
	: name(name), address(address) {

}

NpcArriveEvent::~NpcArriveEvent() {

}

const string& NpcArriveEvent::GetType() const {
	static const string type = "npc_arrive";
	return type;
}

bool NpcArriveEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<NpcArriveEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (name.size() > 0 && other->name.size() > 0) {
		Condition condition;
		condition.ParseCondition(name);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->name);
	}
	if (address.size() > 0 && other->address.size() > 0) {
		Condition condition;
		condition.ParseCondition(address);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->address);
	}
	return result;
}

void NpcArriveEvent::SetName(string name) {
	this->name = name;
}

string NpcArriveEvent::GetName() const {
	return name;
}

void NpcArriveEvent::SetAddress(string address) {
	this->address = address;
}

string NpcArriveEvent::GetAddress() const {
	return address;
}

NPCMeetEvent::NPCMeetEvent(string npc) :
	npc(npc) {

}

NPCMeetEvent::~NPCMeetEvent() {

}

const string& NPCMeetEvent::GetType() const {
	static const string type = "npc_meet";
	return type;
}

bool NPCMeetEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<NPCMeetEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (npc.size() > 0 && other->npc.size() > 0) {
		Condition condition;
		condition.ParseCondition(npc);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->npc);
	}
	return result;
}

void NPCMeetEvent::SetNPC(string npc) {
	this->npc = npc;
}

string NPCMeetEvent::GetNPC() const {
	return npc;
}

CitizenBornEvent::CitizenBornEvent(string name)
	: name(name) {

}

CitizenBornEvent::~CitizenBornEvent() {

}

const string& CitizenBornEvent::GetType() const {
	static const string type = "citizen_born";
	return type;
}

bool CitizenBornEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<CitizenBornEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (name.size() > 0 && other->name.size() > 0) {
		Condition condition;
		condition.ParseCondition(name);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->name);
	}
	return result;
}

void CitizenBornEvent::SetName(string name) {
	this->name = name;
}

string CitizenBornEvent::GetName() const {
	return name;
}

CitizenDeceaseEvent::CitizenDeceaseEvent(string name, string reason)
	: name(name), reason(reason) {

}

CitizenDeceaseEvent::~CitizenDeceaseEvent() {

}

const string& CitizenDeceaseEvent::GetType() const {
	static const string type = "citizen_decease";
	return type;
}

bool CitizenDeceaseEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<CitizenDeceaseEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (name.size() > 0 && other->name.size() > 0) {
		Condition condition;
		condition.ParseCondition(name);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->name);
	}
	if (reason.size() > 0 && other->reason.size() > 0) {
		Condition condition;
		condition.ParseCondition(reason);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->reason);
	}
	return result;
}

void CitizenDeceaseEvent::SetName(string name) {
	this->name = name;
}

string CitizenDeceaseEvent::GetName() const {
	return name;
}

void CitizenDeceaseEvent::SetReason(string reason) {
	this->reason = reason;
}

string CitizenDeceaseEvent::GetReason() const {
	return reason;
}

PlayerInjuredEvent::PlayerInjuredEvent(string wound) :
	wound(wound) {

}

PlayerInjuredEvent::~PlayerInjuredEvent() {

}

const string& PlayerInjuredEvent::GetType() const {
	static const string type = "player_injured";
	return type;
}

bool PlayerInjuredEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<PlayerInjuredEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (wound.size() > 0 && other->wound.size() > 0) {
		Condition condition;
		condition.ParseCondition(wound);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->wound);
	}
	return result;
}

void PlayerInjuredEvent::SetWound(string wound) {
	this->wound = wound;
}

string PlayerInjuredEvent::GetWound() const {
	return wound;
}

PlayerCuredEvent::PlayerCuredEvent(string wound) :
	wound(wound) {

}

PlayerCuredEvent::~PlayerCuredEvent() {

}

const string& PlayerCuredEvent::GetType() const {
	static const string type = "player_cured";
	return type;
}

bool PlayerCuredEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<PlayerCuredEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (wound.size() > 0 && other->wound.size() > 0) {
		Condition condition;
		condition.ParseCondition(wound);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->wound);
	}
	return result;
}

void PlayerCuredEvent::SetWound(string wound) {
	this->wound = wound;
}

string PlayerCuredEvent::GetWound() const {
	return wound;
}

PlayerIllEvent::PlayerIllEvent(string illness) :
	illness(illness) {

}

PlayerIllEvent::~PlayerIllEvent() {

}

const string& PlayerIllEvent::GetType() const {
	static const string type = "player_ill";
	return type;
}

bool PlayerIllEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<PlayerIllEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (illness.size() > 0 && other->illness.size() > 0) {
		Condition condition;
		condition.ParseCondition(illness);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->illness);
	}
	return result;
}

void PlayerIllEvent::SetIllness(string illness) {
	this->illness = illness;
}

string PlayerIllEvent::GetIllness() const {
	return illness;
}

PlayerRecoverEvent::PlayerRecoverEvent(string illness) :
	illness(illness) {

}

PlayerRecoverEvent::~PlayerRecoverEvent() {

}

const string& PlayerRecoverEvent::GetType() const {
	static const string type = "player_recover";
	return type;
}

bool PlayerRecoverEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<PlayerRecoverEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (illness.size() > 0 && other->illness.size() > 0) {
		Condition condition;
		condition.ParseCondition(illness);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->illness);
	}
	return result;
}

void PlayerRecoverEvent::SetIllness(string illness) {
	this->illness = illness;
}

string PlayerRecoverEvent::GetIllness() const {
	return illness;
}

PlayerRestEvent::PlayerRestEvent(int minute) :
	minute(minute) {

}

PlayerRestEvent::~PlayerRestEvent() {

}

const string& PlayerRestEvent::GetType() const {
	static const string type = "player_rest";
	return type;
}

bool PlayerRestEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<PlayerRestEvent*>(e);
	if (!other) return false;

	return minute == other->minute;
}

void PlayerRestEvent::SetMinute(int minute) {
	this->minute = minute;
}

int PlayerRestEvent::GetMinute() const {
	return minute;
}

PlayerSleepEvent::PlayerSleepEvent(int hour) :
	hour(hour) {

}

PlayerSleepEvent::~PlayerSleepEvent() {

}

const string& PlayerSleepEvent::GetType() const {
	static const string type = "player_sleep";
	return type;
}

bool PlayerSleepEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<PlayerSleepEvent*>(e);
	if (!other) return false;

	return hour == other->hour;
}

void PlayerSleepEvent::SetHour(int hour) {
	this->hour = hour;
}

int PlayerSleepEvent::GetHour() const {
	return hour;
}

CultivationChangeEvent::CultivationChangeEvent(string method, int level)
	: method(method), level(level) {

}

CultivationChangeEvent::~CultivationChangeEvent() {

}

const string& CultivationChangeEvent::GetType() const {
	static const string type = "cultivation_change";
	return type;
}

bool CultivationChangeEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<CultivationChangeEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (method.size() > 0 && other->method.size() > 0) {
		Condition condition;
		condition.ParseCondition(method);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->method);
	}
	result = result && (level == other->level);
	return result;
}

void CultivationChangeEvent::SetMethod(string method) {
	this->method = method;
}

string CultivationChangeEvent::GetMethod() const {
	return method;
}

void CultivationChangeEvent::SetLevel(int level) {
	this->level = level;
}

int CultivationChangeEvent::GetLevel() const {
	return level;
}

WantedChangeEvent::WantedChangeEvent(string reason, int level)
	: reason(reason), level(level) {

}

WantedChangeEvent::~WantedChangeEvent() {

}

const string& WantedChangeEvent::GetType() const {
	static const string type = "wanted_change";
	return type;
}

bool WantedChangeEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<WantedChangeEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (reason.size() > 0 && other->reason.size() > 0) {
		Condition condition;
		condition.ParseCondition(reason);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->reason);
	}
	result = result && (level == other->level);
	return result;
}

void WantedChangeEvent::SetReason(string reason) {
	this->reason = reason;
}

string WantedChangeEvent::GetReason() const {
	return reason;
}

void WantedChangeEvent::SetLevel(int level) {
	this->level = level;
}

int WantedChangeEvent::GetLevel() const {
	return level;
}

PlayerArrestedEvent::PlayerArrestedEvent(string reason) :
	reason(reason) {

}

PlayerArrestedEvent::~PlayerArrestedEvent() {

}

const string& PlayerArrestedEvent::GetType() const {
	static const string type = "player_arrested";
	return type;
}

bool PlayerArrestedEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<PlayerArrestedEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (reason.size() > 0 && other->reason.size() > 0) {
		Condition condition;
		condition.ParseCondition(reason);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->reason);
	}
	return result;
}

void PlayerArrestedEvent::SetReason(string reason) {
	this->reason = reason;
}

string PlayerArrestedEvent::GetReason() const {
	return reason;
}

PlayerReleasedEvent::PlayerReleasedEvent(string reason) :
	reason(reason) {

}

PlayerReleasedEvent::~PlayerReleasedEvent() {

}

const string& PlayerReleasedEvent::GetType() const {
	static const string type = "player_released";
	return type;
}

bool PlayerReleasedEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<PlayerReleasedEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (reason.size() > 0 && other->reason.size() > 0) {
		Condition condition;
		condition.ParseCondition(reason);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->reason);
	}
	return result;
}

void PlayerReleasedEvent::SetReason(string reason) {
	this->reason = reason;
}

string PlayerReleasedEvent::GetReason() const {
	return reason;
}

WeatherChangeEvent::WeatherChangeEvent(string weather) :
	weather(weather) {

}

WeatherChangeEvent::~WeatherChangeEvent() {

}

const string& WeatherChangeEvent::GetType() const {
	static const string type = "weather_change";
	return type;
}

bool WeatherChangeEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<WeatherChangeEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (weather.size() > 0 && other->weather.size() > 0) {
		Condition condition;
		condition.ParseCondition(weather);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->weather);
	}
	return result;
}

void WeatherChangeEvent::SetWeather(string weather) {
	this->weather = weather;
}

string WeatherChangeEvent::GetWeather() const {
	return weather;
}

PolicyChangeEvent::PolicyChangeEvent(string policy, bool status)
	: policy(policy), status(status) {

}

PolicyChangeEvent::~PolicyChangeEvent() {

}

const string& PolicyChangeEvent::GetType() const {
	static const string type = "policy_change";
	return type;
}

bool PolicyChangeEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<PolicyChangeEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (policy.size() > 0 && other->policy.size() > 0) {
		Condition condition;
		condition.ParseCondition(policy);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->policy);
	}
	result = result && (status == other->status);
	return result;
}

void PolicyChangeEvent::SetPolicy(string policy) {
	this->policy = policy;
}

string PolicyChangeEvent::GetPolicy() const {
	return policy;
}

void PolicyChangeEvent::SetStatus(bool status) {
	this->status = status;
}

bool PolicyChangeEvent::GetStatus() const {
	return status;
}

UseAssetEvent::UseAssetEvent(string asset) :
	asset(asset) {

}

UseAssetEvent::~UseAssetEvent() {

}

const string& UseAssetEvent::GetType() const {
	static const string type = "use_asset";
	return type;
}

bool UseAssetEvent::Match(Event* e,
	const vector<function<pair<bool, ValueType>(const string&)>>& getValues) {
	if (!e) return false;
	if (GetType() != e->GetType()) return false;

	auto other = dynamic_cast<UseAssetEvent*>(e);
	if (!other) return false;

	bool result = true;
	if (asset.size() > 0 && other->asset.size() > 0) {
		Condition condition;
		condition.ParseCondition(asset);
		result = result && (ToString(condition.EvaluateValue(getValues)) == other->asset);
	}
	return result;
}

void UseAssetEvent::SetAsset(string asset) {
	this->asset = asset;
}

string UseAssetEvent::GetAsset() const {
	return asset;
}


