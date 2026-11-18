#include "change.h"


using namespace std;

Change::Change() :
	condition() {

}

Change::~Change() {

}

const std::string& Change::GetCondition() const {
	return condition;
}

void Change::SetCondition(const std::string& condition) {
	this->condition = condition;
}

ForRangeChange::ForRangeChange(string var, std::string from, std::string to, std::string step,
	vector<const Change*> changes) :
	var(move(var)), from(move(from)), to(move(to)), step(move(step)), changes(move(changes)) {

}

ForRangeChange::~ForRangeChange() {
	// 子Change归for_range自己持有(Script::BuildChanges解析"do"时new出来的)。
	for (const Change* change : changes) delete change;
}

const string& ForRangeChange::GetType() const {
	static const string type = "for_range";
	return type;
}

string ForRangeChange::GetVar() const {
	return var;
}

const std::string& ForRangeChange::GetFrom() const {
	return from;
}

const std::string& ForRangeChange::GetTo() const {
	return to;
}

const std::string& ForRangeChange::GetStep() const {
	return step;
}

const vector<const Change*>& ForRangeChange::GetChanges() const {
	return changes;
}

PlaceHolderChange::PlaceHolderChange() :
	label() {

}

PlaceHolderChange::PlaceHolderChange(std::string label) :
	label(move(label)) {

}

PlaceHolderChange::~PlaceHolderChange() {

}

const string& PlaceHolderChange::GetType() const {
	static const string type = "place_holder";
	return type;
}

void PlaceHolderChange::SetLabel(std::string label) {
	this->label = move(label);
}

const std::string& PlaceHolderChange::GetLabel() const {
	return label;
}

GlobalMessageChange::GlobalMessageChange() :
	message() {

}

GlobalMessageChange::GlobalMessageChange(std::string message, std::string category) :
	message(move(message)), category(move(category)) {

}

GlobalMessageChange::~GlobalMessageChange() {

}

const string& GlobalMessageChange::GetType() const {
	static const string type = "global_message";
	return type;
}

void GlobalMessageChange::SetMessage(std::string message) {
	this->message = move(message);
}

const std::string& GlobalMessageChange::GetMessage() const {
	return message;
}

void GlobalMessageChange::SetCategory(std::string category) {
	this->category = move(category);
}

const std::string& GlobalMessageChange::GetCategory() const {
	return category;
}

DebugPrintChange::DebugPrintChange() :
	message() {

}

DebugPrintChange::DebugPrintChange(std::string message) :
	message(move(message)) {

}

DebugPrintChange::~DebugPrintChange() {

}

const string& DebugPrintChange::GetType() const {
	static const string type = "debug_print";
	return type;
}

void DebugPrintChange::SetMessage(std::string message) {
	this->message = move(message);
}

const std::string& DebugPrintChange::GetMessage() const {
	return message;
}

GameEndChange::GameEndChange() {

}

GameEndChange::~GameEndChange() {

}

const string& GameEndChange::GetType() const {
	static const string type = "game_end";
	return type;
}

SetValueChange::SetValueChange() :
	variable(), value() {

}

SetValueChange::SetValueChange(string variable, std::string value) :
	variable(move(variable)), value(move(value)) {

}

SetValueChange::~SetValueChange() {

}

const string& SetValueChange::GetType() const {
	static const string type = "set_value";
	return type;
}

void SetValueChange::SetVariable(string variable) {
	this->variable = move(variable);
}

string SetValueChange::GetVariable() const {
	return variable;
}

void SetValueChange::SetValue(std::string value) {
	this->value = move(value);
}

const std::string& SetValueChange::GetValue() const {
	return value;
}

GlobalSettingChange::GlobalSettingChange() :
	setting(), value() {

}

GlobalSettingChange::GlobalSettingChange(string setting, std::string value) :
	setting(move(setting)), value(move(value)) {

}

GlobalSettingChange::~GlobalSettingChange() {

}

const string& GlobalSettingChange::GetType() const {
	static const string type = "global_setting";
	return type;
}

void GlobalSettingChange::SetSetting(string setting) {
	this->setting = move(setting);
}

string GlobalSettingChange::GetSetting() const {
	return setting;
}

void GlobalSettingChange::SetValue(std::string value) {
	this->value = move(value);
}

const std::string& GlobalSettingChange::GetValue() const {
	return value;
}

RemoveValueChange::RemoveValueChange() :
	variable() {

}

RemoveValueChange::RemoveValueChange(string variable) :
	variable(move(variable)) {

}

RemoveValueChange::~RemoveValueChange() {

}

const string& RemoveValueChange::GetType() const {
	static const string type = "remove_value";
	return type;
}

void RemoveValueChange::SetVariable(string variable) {
	this->variable = move(variable);
}

string RemoveValueChange::GetVariable() const {
	return variable;
}

DeactivateMilestoneChange::DeactivateMilestoneChange() :
	milestone() {

}

DeactivateMilestoneChange::DeactivateMilestoneChange(string milestone) :
	milestone(move(milestone)) {

}

DeactivateMilestoneChange::~DeactivateMilestoneChange() {

}

const string& DeactivateMilestoneChange::GetType() const {
	static const string type = "deactivate_milestone";
	return type;
}

void DeactivateMilestoneChange::SetMilestone(string milestone) {
	this->milestone = move(milestone);
}

string DeactivateMilestoneChange::GetMilestone() const {
	return milestone;
}

AddOptionChange::AddOptionChange() :
	name(), option() {

}

AddOptionChange::AddOptionChange(std::string name, std::string option, std::string category) :
	name(move(name)), option(move(option)), category(move(category)) {

}

AddOptionChange::~AddOptionChange() {

}

const string& AddOptionChange::GetType() const {
	static const string type = "add_option";
	return type;
}

void AddOptionChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& AddOptionChange::GetName() const {
	return name;
}

void AddOptionChange::SetOption(std::string option) {
	this->option = move(option);
}

const std::string& AddOptionChange::GetOption() const {
	return option;
}

void AddOptionChange::SetCategory(std::string category) {
	this->category = move(category);
}

const std::string& AddOptionChange::GetCategory() const {
	return category;
}

RemoveOptionChange::RemoveOptionChange() :
	name(), option() {

}

RemoveOptionChange::RemoveOptionChange(std::string name, std::string option, std::string category) :
	name(move(name)), option(move(option)), category(move(category)) {

}

RemoveOptionChange::~RemoveOptionChange() {

}

const string& RemoveOptionChange::GetType() const {
	static const string type = "remove_option";
	return type;
}

void RemoveOptionChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& RemoveOptionChange::GetName() const {
	return name;
}

void RemoveOptionChange::SetOption(std::string option) {
	this->option = move(option);
}

const std::string& RemoveOptionChange::GetOption() const {
	return option;
}

void RemoveOptionChange::SetCategory(std::string category) {
	this->category = move(category);
}

const std::string& RemoveOptionChange::GetCategory() const {
	return category;
}

SpawnNpcChange::SpawnNpcChange() :
	avatar(), name(), gender(), birthday(), height(), weight(),
	nick(), deposit(), phone(), home(), jobs(), scheduler(), economy() {

}

SpawnNpcChange::SpawnNpcChange(std::string avatar, std::string name, std::string gender, std::string birthday, std::string height, std::string weight,
	std::string nick, std::string deposit, std::string phone, std::string home, vector<std::string> jobs, std::string scheduler,
	std::string economy) :
	avatar(move(avatar)), name(move(name)), gender(move(gender)), birthday(move(birthday)), height(move(height)), weight(move(weight)),
	nick(move(nick)), deposit(move(deposit)), phone(move(phone)), home(move(home)), jobs(move(jobs)), scheduler(move(scheduler)), economy(move(economy)) {

}

SpawnNpcChange::~SpawnNpcChange() {

}

const string& SpawnNpcChange::GetType() const {
	static const string type = "spawn_npc";
	return type;
}

void SpawnNpcChange::SetAvatar(std::string avatar) {
	this->avatar = move(avatar);
}

const std::string& SpawnNpcChange::GetAvatar() const {
	return avatar;
}

void SpawnNpcChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& SpawnNpcChange::GetName() const {
	return name;
}

void SpawnNpcChange::SetGender(std::string gender) {
	this->gender = move(gender);
}

const std::string& SpawnNpcChange::GetGender() const {
	return gender;
}

void SpawnNpcChange::SetBirthday(std::string birthday) {
	this->birthday = move(birthday);
}

const std::string& SpawnNpcChange::GetBirthday() const {
	return birthday;
}

void SpawnNpcChange::SetHeight(std::string height) {
	this->height = move(height);
}

const std::string& SpawnNpcChange::GetHeight() const {
	return height;
}

void SpawnNpcChange::SetWeight(std::string weight) {
	this->weight = move(weight);
}

const std::string& SpawnNpcChange::GetWeight() const {
	return weight;
}

void SpawnNpcChange::SetNick(std::string nick) {
	this->nick = move(nick);
}

const std::string& SpawnNpcChange::GetNick() const {
	return nick;
}

void SpawnNpcChange::SetDeposit(std::string deposit) {
	this->deposit = move(deposit);
}

const std::string& SpawnNpcChange::GetDeposit() const {
	return deposit;
}

void SpawnNpcChange::SetPhone(std::string phone) {
	this->phone = move(phone);
}

const std::string& SpawnNpcChange::GetPhone() const {
	return phone;
}

void SpawnNpcChange::SetHome(std::string home) {
	this->home = move(home);
}

const std::string& SpawnNpcChange::GetHome() const {
	return home;
}

void SpawnNpcChange::SetJobs(vector<std::string> jobs) {
	this->jobs = move(jobs);
}

const vector<std::string>& SpawnNpcChange::GetJobs() const {
	return jobs;
}

void SpawnNpcChange::SetScheduler(std::string scheduler) {
	this->scheduler = move(scheduler);
}

const std::string& SpawnNpcChange::GetScheduler() const {
	return scheduler;
}

void SpawnNpcChange::SetEconomy(std::string economy) {
	this->economy = move(economy);
}

const std::string& SpawnNpcChange::GetEconomy() const {
	return economy;
}

RemoveNpcChange::RemoveNpcChange() :
	name() {

}

RemoveNpcChange::RemoveNpcChange(std::string name) :
	name(move(name)) {

}

RemoveNpcChange::~RemoveNpcChange() {

}

const string& RemoveNpcChange::GetType() const {
	static const string type = "remove_npc";
	return type;
}

void RemoveNpcChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& RemoveNpcChange::GetName() const {
	return name;
}

NPCReviveChange::NPCReviveChange() :
	name(), health() {

}

NPCReviveChange::NPCReviveChange(std::string name, std::string health) :
	name(move(name)), health(move(health)) {

}

NPCReviveChange::~NPCReviveChange() {

}

const string& NPCReviveChange::GetType() const {
	static const string type = "npc_revive";
	return type;
}

void NPCReviveChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& NPCReviveChange::GetName() const {
	return name;
}

void NPCReviveChange::SetHealth(std::string health) {
	this->health = move(health);
}

const std::string& NPCReviveChange::GetHealth() const {
	return health;
}

NPCTeleportChange::NPCTeleportChange() :
	name(), destination() {

}

NPCTeleportChange::NPCTeleportChange(std::string name, std::string destination) :
	name(move(name)), destination(move(destination)) {

}

NPCTeleportChange::~NPCTeleportChange() {

}

const string& NPCTeleportChange::GetType() const {
	static const string type = "npc_teleport";
	return type;
}

void NPCTeleportChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& NPCTeleportChange::GetName() const {
	return name;
}

void NPCTeleportChange::SetDestination(std::string destination) {
	this->destination = move(destination);
}

const std::string& NPCTeleportChange::GetDestination() const {
	return destination;
}

NPCNavigateChange::NPCNavigateChange() :
	name(), destination() {

}

NPCNavigateChange::NPCNavigateChange(string name, string destination) :
	name(move(name)), destination(move(destination)) {

}

NPCNavigateChange::~NPCNavigateChange() {

}

const string& NPCNavigateChange::GetType() const {
	static const string type = "npc_navigate";
	return type;
}

void NPCNavigateChange::SetName(string name) {
	this->name = move(name);
}

const string& NPCNavigateChange::GetName() const {
	return name;
}

void NPCNavigateChange::SetDestination(string destination) {
	this->destination = move(destination);
}

const string& NPCNavigateChange::GetDestination() const {
	return destination;
}

AssignTaskChange::AssignTaskChange() :
	task(), destination(), vehicle(), roundTrip(), worker() {

}

AssignTaskChange::AssignTaskChange(string task, string destination, string vehicle, string roundTrip, string worker) :
	task(move(task)), destination(move(destination)), vehicle(move(vehicle)), roundTrip(move(roundTrip)), worker(move(worker)) {

}

AssignTaskChange::~AssignTaskChange() {

}

const string& AssignTaskChange::GetType() const {
	static const string type = "assign_task";
	return type;
}

const string& AssignTaskChange::GetTask() const {
	return task;
}

const string& AssignTaskChange::GetDestination() const {
	return destination;
}

const string& AssignTaskChange::GetVehicle() const {
	return vehicle;
}

const string& AssignTaskChange::GetRoundTrip() const {
	return roundTrip;
}

const string& AssignTaskChange::GetWorker() const {
	return worker;
}

TeleportPlayerChange::TeleportPlayerChange() :
	destination() {

}

TeleportPlayerChange::TeleportPlayerChange(std::string destination) :
	destination(move(destination)) {

}

TeleportPlayerChange::~TeleportPlayerChange() {

}

const string& TeleportPlayerChange::GetType() const {
	static const string type = "teleport_player";
	return type;
}

void TeleportPlayerChange::SetDestination(std::string destination) {
	this->destination = move(destination);
}

const std::string& TeleportPlayerChange::GetDestination() const {
	return destination;
}

OpenShopChange::OpenShopChange() :
	saler() {

}

OpenShopChange::OpenShopChange(std::string saler) :
	saler(move(saler)) {

}

OpenShopChange::~OpenShopChange() {

}

const string& OpenShopChange::GetType() const {
	static const string type = "open_shop";
	return type;
}

void OpenShopChange::SetSaler(std::string saler) {
	this->saler = move(saler);
}

const std::string& OpenShopChange::GetSaler() const {
	return saler;
}

StartPuzzleChange::StartPuzzleChange() :
	puzzle(),
	param() {

}

StartPuzzleChange::StartPuzzleChange(std::string puzzle) :
	puzzle(move(puzzle)),
	param() {

}

StartPuzzleChange::StartPuzzleChange(std::string puzzle, std::string param) :
	puzzle(move(puzzle)),
	param(move(param)) {

}

StartPuzzleChange::~StartPuzzleChange() {

}

const string& StartPuzzleChange::GetType() const {
	static const string type = "start_puzzle";
	return type;
}

void StartPuzzleChange::SetPuzzle(std::string puzzle) {
	this->puzzle = move(puzzle);
}

const std::string& StartPuzzleChange::GetPuzzle() const {
	return puzzle;
}

void StartPuzzleChange::SetParam(std::string param) {
	this->param = move(param);
}

const std::string& StartPuzzleChange::GetParam() const {
	return param;
}

PayWageChange::PayWageChange() :
	amount() {

}

PayWageChange::PayWageChange(std::string amount) :
	amount(move(amount)) {

}

PayWageChange::~PayWageChange() {

}

const string& PayWageChange::GetType() const {
	static const string type = "pay_wage";
	return type;
}

void PayWageChange::SetAmount(std::string amount) {
	this->amount = move(amount);
}

const std::string& PayWageChange::GetAmount() const {
	return amount;
}

EnterVehicleChange::EnterVehicleChange() :
	vehicle() {

}

EnterVehicleChange::EnterVehicleChange(std::string vehicle) :
	vehicle(move(vehicle)) {

}

EnterVehicleChange::~EnterVehicleChange() {

}

const string& EnterVehicleChange::GetType() const {
	static const string type = "enter_vehicle";
	return type;
}

void EnterVehicleChange::SetVehicle(std::string vehicle) {
	this->vehicle = move(vehicle);
}

const std::string& EnterVehicleChange::GetVehicle() const {
	return vehicle;
}

ShowRouteChange::ShowRouteChange() : target() {

}

ShowRouteChange::ShowRouteChange(std::string target) : target(move(target)) {

}

ShowRouteChange::~ShowRouteChange() {

}

const string& ShowRouteChange::GetType() const {
	static const string type = "show_route";
	return type;
}

const std::string& ShowRouteChange::GetTarget() const {
	return target;
}

VisualizeNavigationChange::VisualizeNavigationChange() :
	address() {

}

VisualizeNavigationChange::VisualizeNavigationChange(std::string address) :
	address(move(address)) {

}

VisualizeNavigationChange::~VisualizeNavigationChange() {

}

const string& VisualizeNavigationChange::GetType() const {
	static const string type = "visualize_navigation";
	return type;
}

void VisualizeNavigationChange::SetAddress(std::string address) {
	this->address = move(address);
}

const std::string& VisualizeNavigationChange::GetAddress() const {
	return address;
}

VehicleCommandChange::VehicleCommandChange() :
	vehicle(), command() {

}

VehicleCommandChange::VehicleCommandChange(std::string vehicle, std::string command) :
	vehicle(move(vehicle)), command(move(command)) {

}

VehicleCommandChange::~VehicleCommandChange() {

}

const string& VehicleCommandChange::GetType() const {
	static const string type = "vehicle_command";
	return type;
}

void VehicleCommandChange::SetVehicle(std::string vehicle) {
	this->vehicle = move(vehicle);
}

const std::string& VehicleCommandChange::GetVehicle() const {
	return vehicle;
}

void VehicleCommandChange::SetCommand(std::string command) {
	this->command = move(command);
}

const std::string& VehicleCommandChange::GetCommand() const {
	return command;
}

VehicleNavigateChange::VehicleNavigateChange() :
	vehicle(), destination() {

}

VehicleNavigateChange::VehicleNavigateChange(std::string vehicle, std::string destination) :
	vehicle(move(vehicle)), destination(move(destination)) {

}

VehicleNavigateChange::~VehicleNavigateChange() {

}

const string& VehicleNavigateChange::GetType() const {
	static const string type = "vehicle_navigate";
	return type;
}

void VehicleNavigateChange::SetVehicle(std::string vehicle) {
	this->vehicle = move(vehicle);
}

const std::string& VehicleNavigateChange::GetVehicle() const {
	return vehicle;
}

void VehicleNavigateChange::SetDestination(std::string destination) {
	this->destination = move(destination);
}

const std::string& VehicleNavigateChange::GetDestination() const {
	return destination;
}

LeaveVehicleChange::LeaveVehicleChange() :
	vehicle() {

}

LeaveVehicleChange::LeaveVehicleChange(std::string vehicle) :
	vehicle(move(vehicle)) {

}

LeaveVehicleChange::~LeaveVehicleChange() {

}

const string& LeaveVehicleChange::GetType() const {
	static const string type = "leave_vehicle";
	return type;
}

void LeaveVehicleChange::SetVehicle(std::string vehicle) {
	this->vehicle = move(vehicle);
}

const std::string& LeaveVehicleChange::GetVehicle() const {
	return vehicle;
}

BoardVehicleChange::BoardVehicleChange() :
	vehicle() {

}

BoardVehicleChange::BoardVehicleChange(std::string vehicle) :
	vehicle(move(vehicle)) {

}

BoardVehicleChange::~BoardVehicleChange() {

}

const string& BoardVehicleChange::GetType() const {
	static const string type = "board_vehicle";
	return type;
}

void BoardVehicleChange::SetVehicle(std::string vehicle) {
	this->vehicle = move(vehicle);
}

const std::string& BoardVehicleChange::GetVehicle() const {
	return vehicle;
}

ForceLeaveChange::ForceLeaveChange() :
	vehicle() {

}

ForceLeaveChange::ForceLeaveChange(std::string vehicle) :
	vehicle(move(vehicle)) {

}

ForceLeaveChange::~ForceLeaveChange() {

}

const string& ForceLeaveChange::GetType() const {
	static const string type = "force_leave";
	return type;
}

void ForceLeaveChange::SetVehicle(std::string vehicle) {
	this->vehicle = move(vehicle);
}

const std::string& ForceLeaveChange::GetVehicle() const {
	return vehicle;
}

SetDoorChange::SetDoorChange() :
	name(), access() {

}

SetDoorChange::SetDoorChange(std::string name, std::string access) :
	name(move(name)), access(move(access)) {

}

SetDoorChange::~SetDoorChange() {

}

const string& SetDoorChange::GetType() const {
	static const string type = "set_door";
	return type;
}

void SetDoorChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& SetDoorChange::GetName() const {
	return name;
}

void SetDoorChange::SetAccess(std::string access) {
	this->access = move(access);
}

const std::string& SetDoorChange::GetAccess() const {
	return access;
}

RecordObjectChange::RecordObjectChange() :
	object(), kind(), policy(), variable() {

}

RecordObjectChange::RecordObjectChange(std::string object, std::string kind, std::string policy, std::string variable) :
	object(move(object)), kind(move(kind)), policy(move(policy)), variable(move(variable)) {

}

RecordObjectChange::~RecordObjectChange() {

}

const string& RecordObjectChange::GetType() const {
	static const string type = "record_object";
	return type;
}

void RecordObjectChange::SetObject(std::string object) { this->object = move(object); }
const std::string& RecordObjectChange::GetObject() const { return object; }
void RecordObjectChange::SetKind(std::string kind) { this->kind = move(kind); }
const std::string& RecordObjectChange::GetKind() const { return kind; }
void RecordObjectChange::SetPolicy(std::string policy) { this->policy = move(policy); }
const std::string& RecordObjectChange::GetPolicy() const { return policy; }
void RecordObjectChange::SetVariable(std::string variable) { this->variable = move(variable); }
const std::string& RecordObjectChange::GetVariable() const { return variable; }

RegisterDoorChange::RegisterDoorChange() :
	name(), citizen(), pass() {

}

RegisterDoorChange::RegisterDoorChange(std::string name, std::string citizen, std::string pass) :
	name(move(name)), citizen(move(citizen)), pass(move(pass)) {

}

RegisterDoorChange::~RegisterDoorChange() {

}

const string& RegisterDoorChange::GetType() const {
	static const string type = "register_door";
	return type;
}

void RegisterDoorChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& RegisterDoorChange::GetName() const {
	return name;
}

void RegisterDoorChange::SetCitizen(std::string citizen) {
	this->citizen = move(citizen);
}

const std::string& RegisterDoorChange::GetCitizen() const {
	return citizen;
}

void RegisterDoorChange::SetPass(std::string pass) {
	this->pass = move(pass);
}

const std::string& RegisterDoorChange::GetPass() const {
	return pass;
}

JumpTimeChange::JumpTimeChange() :
	time(), mode() {

}

JumpTimeChange::JumpTimeChange(std::string time, std::string mode) :
	time(move(time)), mode(move(mode)) {

}

JumpTimeChange::~JumpTimeChange() {

}

const string& JumpTimeChange::GetType() const {
	static const string type = "jump_time";
	return type;
}

void JumpTimeChange::SetTime(std::string time) {
	this->time = move(time);
}

const std::string& JumpTimeChange::GetTime() const {
	return time;
}

void JumpTimeChange::SetMode(std::string mode) {
	this->mode = move(mode);
}

const std::string& JumpTimeChange::GetMode() const {
	return mode;
}

CreateTimerChange::CreateTimerChange() :
	name(), time(), category(), label(), mode() {

}

CreateTimerChange::CreateTimerChange(std::string name, std::string time, std::string category, std::string label,
	std::string mode) :
	name(move(name)), time(move(time)), category(move(category)), label(move(label)), mode(move(mode)) {

}

CreateTimerChange::~CreateTimerChange() {

}

const string& CreateTimerChange::GetType() const {
	static const string type = "create_timer";
	return type;
}

void CreateTimerChange::SetMode(std::string mode) {
	this->mode = move(mode);
}

const std::string& CreateTimerChange::GetMode() const {
	return mode;
}

void CreateTimerChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& CreateTimerChange::GetName() const {
	return name;
}

void CreateTimerChange::SetTime(std::string time) {
	this->time = move(time);
}

const std::string& CreateTimerChange::GetTime() const {
	return time;
}

void CreateTimerChange::SetCategory(std::string category) {
	this->category = move(category);
}

const std::string& CreateTimerChange::GetCategory() const {
	return category;
}

void CreateTimerChange::SetLabel(std::string label) {
	this->label = move(label);
}

const std::string& CreateTimerChange::GetLabel() const {
	return label;
}

RemoveTimerChange::RemoveTimerChange() :
	name() {

}

RemoveTimerChange::RemoveTimerChange(std::string name) :
	name(move(name)) {

}

RemoveTimerChange::~RemoveTimerChange() {

}

const string& RemoveTimerChange::GetType() const {
	static const string type = "remove_timer";
	return type;
}

void RemoveTimerChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& RemoveTimerChange::GetName() const {
	return name;
}

LaunchElevatorChange::LaunchElevatorChange() :
	building(), elevator(), command() {

}

LaunchElevatorChange::LaunchElevatorChange(std::string building, std::string elevator, std::string command) :
	building(move(building)), elevator(move(elevator)), command(move(command)) {

}

LaunchElevatorChange::~LaunchElevatorChange() {

}

const string& LaunchElevatorChange::GetType() const {
	static const string type = "launch_elevator";
	return type;
}

void LaunchElevatorChange::SetBuilding(std::string building) {
	this->building = move(building);
}

const std::string& LaunchElevatorChange::GetBuilding() const {
	return building;
}

void LaunchElevatorChange::SetElevator(std::string elevator) {
	this->elevator = move(elevator);
}

const std::string& LaunchElevatorChange::GetElevator() const {
	return elevator;
}

void LaunchElevatorChange::SetCommand(std::string command) {
	this->command = move(command);
}

const std::string& LaunchElevatorChange::GetCommand() const {
	return command;
}

PlayVideoChange::PlayVideoChange() :
	path() {

}

PlayVideoChange::PlayVideoChange(std::string path) :
	path(move(path)) {

}

PlayVideoChange::~PlayVideoChange() {

}

const string& PlayVideoChange::GetType() const {
	static const string type = "play_video";
	return type;
}

void PlayVideoChange::SetPath(std::string path) {
	this->path = move(path);
}

const std::string& PlayVideoChange::GetPath() const {
	return path;
}

PlayBgmChange::PlayBgmChange() :
	bgm(), loop() {

}

PlayBgmChange::PlayBgmChange(std::string bgm, std::string loop) :
	bgm(move(bgm)), loop(move(loop)) {

}

PlayBgmChange::~PlayBgmChange() {

}

const string& PlayBgmChange::GetType() const {
	static const string type = "play_bgm";
	return type;
}

void PlayBgmChange::SetBgm(std::string bgm) {
	this->bgm = move(bgm);
}

const std::string& PlayBgmChange::GetBgm() const {
	return bgm;
}

void PlayBgmChange::SetLoop(std::string loop) {
	this->loop = move(loop);
}

const std::string& PlayBgmChange::GetLoop() const {
	return loop;
}

StopBgmChange::StopBgmChange() {

}

StopBgmChange::~StopBgmChange() {

}

const string& StopBgmChange::GetType() const {
	static const string type = "stop_bgm";
	return type;
}

BankTransactionChange::BankTransactionChange() :
	name(), amount() {

}

BankTransactionChange::BankTransactionChange(std::string name, std::string amount) :
	name(move(name)), amount(move(amount)) {

}

BankTransactionChange::~BankTransactionChange() {

}

const string& BankTransactionChange::GetType() const {
	static const string type = "bank_transaction";
	return type;
}

void BankTransactionChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& BankTransactionChange::GetName() const {
	return name;
}

void BankTransactionChange::SetAmount(std::string amount) {
	this->amount = move(amount);
}

const std::string& BankTransactionChange::GetAmount() const {
	return amount;
}

GiveEstateChange::GiveEstateChange() :
	estate(), name(), force() {

}

GiveEstateChange::GiveEstateChange(std::string estate, std::string name, std::string force) :
	estate(move(estate)), name(move(name)), force(move(force)) {

}

GiveEstateChange::~GiveEstateChange() {

}

const string& GiveEstateChange::GetType() const {
	static const string type = "give_estate";
	return type;
}

void GiveEstateChange::SetEstate(std::string estate) {
	this->estate = move(estate);
}

const std::string& GiveEstateChange::GetEstate() const {
	return estate;
}

void GiveEstateChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& GiveEstateChange::GetName() const {
	return name;
}

void GiveEstateChange::SetForce(std::string force) {
	this->force = move(force);
}

const std::string& GiveEstateChange::GetForce() const {
	return force;
}

RemoveEstateChange::RemoveEstateChange() :
	estate(), name() {

}

RemoveEstateChange::RemoveEstateChange(std::string estate, std::string name) :
	estate(move(estate)), name(move(name)) {

}

RemoveEstateChange::~RemoveEstateChange() {

}

const string& RemoveEstateChange::GetType() const {
	static const string type = "remove_estate";
	return type;
}

void RemoveEstateChange::SetEstate(std::string estate) {
	this->estate = move(estate);
}

const std::string& RemoveEstateChange::GetEstate() const {
	return estate;
}

void RemoveEstateChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& RemoveEstateChange::GetName() const {
	return name;
}

GiveVehicleChange::GiveVehicleChange() :
	vehicle(), name(), force() {

}

GiveVehicleChange::GiveVehicleChange(std::string vehicle, std::string name, std::string force) :
	vehicle(move(vehicle)), name(move(name)), force(move(force)) {

}

GiveVehicleChange::~GiveVehicleChange() {

}

const string& GiveVehicleChange::GetType() const {
	static const string type = "give_vehicle";
	return type;
}

void GiveVehicleChange::SetVehicle(std::string vehicle) {
	this->vehicle = move(vehicle);
}

const std::string& GiveVehicleChange::GetVehicle() const {
	return vehicle;
}

void GiveVehicleChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& GiveVehicleChange::GetName() const {
	return name;
}

void GiveVehicleChange::SetForce(std::string force) {
	this->force = move(force);
}

const std::string& GiveVehicleChange::GetForce() const {
	return force;
}

RemoveVehicleChange::RemoveVehicleChange() :
	vehicle(), name() {

}

RemoveVehicleChange::RemoveVehicleChange(std::string vehicle, std::string name) :
	vehicle(move(vehicle)), name(move(name)) {

}

RemoveVehicleChange::~RemoveVehicleChange() {

}

const string& RemoveVehicleChange::GetType() const {
	static const string type = "remove_vehicle";
	return type;
}

void RemoveVehicleChange::SetVehicle(std::string vehicle) {
	this->vehicle = move(vehicle);
}

const std::string& RemoveVehicleChange::GetVehicle() const {
	return vehicle;
}

void RemoveVehicleChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& RemoveVehicleChange::GetName() const {
	return name;
}

PlayerInjuredChange::PlayerInjuredChange() :
	wound() {

}

PlayerInjuredChange::PlayerInjuredChange(std::string wound) :
	wound(move(wound)) {

}

PlayerInjuredChange::~PlayerInjuredChange() {

}

const string& PlayerInjuredChange::GetType() const {
	static const string type = "player_injured";
	return type;
}

void PlayerInjuredChange::SetWound(std::string wound) {
	this->wound = move(wound);
}

const std::string& PlayerInjuredChange::GetWound() const {
	return wound;
}

PlayerCuredChange::PlayerCuredChange() :
	wound() {

}

PlayerCuredChange::PlayerCuredChange(std::string wound) :
	wound(move(wound)) {

}

PlayerCuredChange::~PlayerCuredChange() {

}

const string& PlayerCuredChange::GetType() const {
	static const string type = "player_cured";
	return type;
}

void PlayerCuredChange::SetWound(std::string wound) {
	this->wound = move(wound);
}

const std::string& PlayerCuredChange::GetWound() const {
	return wound;
}

PlayerIllChange::PlayerIllChange() :
	illness() {

}

PlayerIllChange::PlayerIllChange(std::string illness) :
	illness(move(illness)) {

}

PlayerIllChange::~PlayerIllChange() {

}

const string& PlayerIllChange::GetType() const {
	static const string type = "player_ill";
	return type;
}

void PlayerIllChange::SetIllness(std::string illness) {
	this->illness = move(illness);
}

const std::string& PlayerIllChange::GetIllness() const {
	return illness;
}

PlayerRecoverChange::PlayerRecoverChange() :
	illness() {

}

PlayerRecoverChange::PlayerRecoverChange(std::string illness) :
	illness(move(illness)) {

}

PlayerRecoverChange::~PlayerRecoverChange() {

}

const string& PlayerRecoverChange::GetType() const {
	static const string type = "player_recover";
	return type;
}

void PlayerRecoverChange::SetIllness(std::string illness) {
	this->illness = move(illness);
}

const std::string& PlayerRecoverChange::GetIllness() const {
	return illness;
}

PlayerSleepChange::PlayerSleepChange() :
	hour() {

}

PlayerSleepChange::PlayerSleepChange(std::string hour) :
	hour(move(hour)) {

}

PlayerSleepChange::~PlayerSleepChange() {

}

const string& PlayerSleepChange::GetType() const {
	static const string type = "player_sleep";
	return type;
}

void PlayerSleepChange::SetHour(std::string hour) {
	this->hour = move(hour);
}

const std::string& PlayerSleepChange::GetHour() const {
	return hour;
}

ChangeTimeChange::ChangeTimeChange() :
	delta() {

}

ChangeTimeChange::ChangeTimeChange(std::string delta) :
	delta(move(delta)) {

}

ChangeTimeChange::~ChangeTimeChange() {

}

const string& ChangeTimeChange::GetType() const {
	static const string type = "change_time";
	return type;
}

void ChangeTimeChange::SetDelta(std::string delta) {
	this->delta = move(delta);
}

const std::string& ChangeTimeChange::GetDelta() const {
	return delta;
}

ChangeWantedChange::ChangeWantedChange() :
	reason(), level() {

}

ChangeWantedChange::ChangeWantedChange(std::string reason, std::string level) :
	reason(move(reason)), level(move(level)) {

}

ChangeWantedChange::~ChangeWantedChange() {

}

const string& ChangeWantedChange::GetType() const {
	static const string type = "change_wanted";
	return type;
}

void ChangeWantedChange::SetReason(std::string reason) {
	this->reason = move(reason);
}

const std::string& ChangeWantedChange::GetReason() const {
	return reason;
}

void ChangeWantedChange::SetLevel(std::string level) {
	this->level = move(level);
}

const std::string& ChangeWantedChange::GetLevel() const {
	return level;
}

ChangeWeatherChange::ChangeWeatherChange() :
	weather() {

}

ChangeWeatherChange::ChangeWeatherChange(std::string weather) :
	weather(move(weather)) {

}

ChangeWeatherChange::~ChangeWeatherChange() {

}

const string& ChangeWeatherChange::GetType() const {
	static const string type = "change_weather";
	return type;
}

void ChangeWeatherChange::SetWeather(std::string weather) {
	this->weather = move(weather);
}

const std::string& ChangeWeatherChange::GetWeather() const {
	return weather;
}

ChangeControlChange::ChangeControlChange() :
	name() {

}

ChangeControlChange::ChangeControlChange(std::string name) :
	name(move(name)) {

}

ChangeControlChange::~ChangeControlChange() {

}

const string& ChangeControlChange::GetType() const {
	static const string type = "change_control";
	return type;
}

void ChangeControlChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& ChangeControlChange::GetName() const {
	return name;
}

PickAssetChange::PickAssetChange() :
	asset() {

}

PickAssetChange::PickAssetChange(std::string asset) :
	asset(move(asset)) {

}

PickAssetChange::~PickAssetChange() {

}

const string& PickAssetChange::GetType() const {
	static const string type = "pick_asset";
	return type;
}

void PickAssetChange::SetAsset(std::string asset) {
	this->asset = move(asset);
}

const std::string& PickAssetChange::GetAsset() const {
	return asset;
}

GrantAssetChange::GrantAssetChange() :
	type(), count() {

}

GrantAssetChange::GrantAssetChange(std::string type, std::string count) :
	type(move(type)), count(move(count)) {

}

GrantAssetChange::~GrantAssetChange() {

}

const string& GrantAssetChange::GetType() const {
	static const string type = "grant_asset";
	return type;
}

void GrantAssetChange::SetAssetType(std::string type) {
	this->type = move(type);
}

const std::string& GrantAssetChange::GetAssetType() const {
	return type;
}

void GrantAssetChange::SetCount(std::string count) {
	this->count = move(count);
}

const std::string& GrantAssetChange::GetCount() const {
	return count;
}

DeductAssetChange::DeductAssetChange() :
	type(), count() {

}

DeductAssetChange::DeductAssetChange(std::string type, std::string count) :
	type(move(type)), count(move(count)) {

}

DeductAssetChange::~DeductAssetChange() {

}

const string& DeductAssetChange::GetType() const {
	static const string type = "deduct_asset";
	return type;
}

void DeductAssetChange::SetAssetType(std::string type) {
	this->type = move(type);
}

const std::string& DeductAssetChange::GetAssetType() const {
	return type;
}

void DeductAssetChange::SetCount(std::string count) {
	this->count = move(count);
}

const std::string& DeductAssetChange::GetCount() const {
	return count;
}

DropAssetChange::DropAssetChange() :
	type(), count() {

}

DropAssetChange::DropAssetChange(std::string type, std::string count) :
	type(move(type)), count(move(count)) {

}

DropAssetChange::~DropAssetChange() {

}

const string& DropAssetChange::GetType() const {
	static const string type = "drop_asset";
	return type;
}

void DropAssetChange::SetAssetType(std::string type) {
	this->type = move(type);
}

const std::string& DropAssetChange::GetAssetType() const {
	return type;
}

void DropAssetChange::SetCount(std::string count) {
	this->count = move(count);
}

const std::string& DropAssetChange::GetCount() const {
	return count;
}


SpawnEffectChange::SpawnEffectChange() :
	effect(), scale(), asset() {

}

SpawnEffectChange::SpawnEffectChange(std::string effect, std::string scale, std::string asset, std::string position,
	std::string normal) :
	effect(move(effect)), scale(move(scale)), asset(move(asset)), position(move(position)), normal(move(normal)) {

}

SpawnEffectChange::~SpawnEffectChange() {

}

const string& SpawnEffectChange::GetType() const {
	static const string type = "spawn_effect";
	return type;
}

void SpawnEffectChange::SetEffect(std::string effect) {
	this->effect = move(effect);
}

const std::string& SpawnEffectChange::GetEffect() const {
	return effect;
}

void SpawnEffectChange::SetScale(std::string scale) {
	this->scale = move(scale);
}

const std::string& SpawnEffectChange::GetScale() const {
	return scale;
}

void SpawnEffectChange::SetAsset(std::string asset) {
	this->asset = move(asset);
}

const std::string& SpawnEffectChange::GetAsset() const {
	return asset;
}

void SpawnEffectChange::SetPosition(std::string position) {
	this->position = move(position);
}

const std::string& SpawnEffectChange::GetPosition() const {
	return position;
}

void SpawnEffectChange::SetNormal(std::string normal) {
	this->normal = move(normal);
}

const std::string& SpawnEffectChange::GetNormal() const {
	return normal;
}

SpawnActorChange::SpawnActorChange() :
	actor(), position(), direction(), scale() {

}

SpawnActorChange::SpawnActorChange(std::string actor, std::string position, std::string direction, std::string scale) :
	actor(move(actor)), position(move(position)), direction(move(direction)), scale(move(scale)) {

}

SpawnActorChange::~SpawnActorChange() {

}

const string& SpawnActorChange::GetType() const {
	static const string type = "spawn_actor";
	return type;
}

void SpawnActorChange::SetActor(std::string actor) {
	this->actor = move(actor);
}

const std::string& SpawnActorChange::GetActor() const {
	return actor;
}

void SpawnActorChange::SetPosition(std::string position) {
	this->position = move(position);
}

const std::string& SpawnActorChange::GetPosition() const {
	return position;
}

void SpawnActorChange::SetDirection(std::string direction) {
	this->direction = move(direction);
}

const std::string& SpawnActorChange::GetDirection() const {
	return direction;
}

void SpawnActorChange::SetScale(std::string scale) {
	this->scale = move(scale);
}

const std::string& SpawnActorChange::GetScale() const {
	return scale;
}

DestroyAssetChange::DestroyAssetChange() :
	asset() {

}

DestroyAssetChange::DestroyAssetChange(std::string asset) :
	asset(move(asset)) {

}

DestroyAssetChange::~DestroyAssetChange() {

}

const string& DestroyAssetChange::GetType() const {
	static const string type = "destroy_asset";
	return type;
}

void DestroyAssetChange::SetAsset(std::string asset) {
	this->asset = move(asset);
}

const std::string& DestroyAssetChange::GetAsset() const {
	return asset;
}

SingleDamageChange::SingleDamageChange() :
	target(), damage(), source(), weapon(), part() {

}

SingleDamageChange::SingleDamageChange(std::string target, std::string damage, std::string source, std::string weapon, std::string part) :
	target(move(target)), damage(move(damage)), source(move(source)), weapon(move(weapon)), part(move(part)) {

}

SingleDamageChange::~SingleDamageChange() {

}

const string& SingleDamageChange::GetType() const {
	static const string type = "single_damage";
	return type;
}

void SingleDamageChange::SetTarget(std::string target) {
	this->target = move(target);
}

const std::string& SingleDamageChange::GetTarget() const {
	return target;
}

void SingleDamageChange::SetDamage(std::string damage) {
	this->damage = move(damage);
}

const std::string& SingleDamageChange::GetDamage() const {
	return damage;
}

void SingleDamageChange::SetSource(std::string source) {
	this->source = move(source);
}

const std::string& SingleDamageChange::GetSource() const {
	return source;
}

void SingleDamageChange::SetWeapon(std::string weapon) {
	this->weapon = move(weapon);
}

const std::string& SingleDamageChange::GetWeapon() const {
	return weapon;
}

void SingleDamageChange::SetPart(std::string part) {
	this->part = move(part);
}

const std::string& SingleDamageChange::GetPart() const {
	return part;
}

AreaDamageChange::AreaDamageChange() :
	position(), radius(), damage(), occlusion(), power(), source(), weapon() {

}

AreaDamageChange::AreaDamageChange(std::string position, std::string radius, std::string damage, std::string occlusion,
	std::string power, std::string source, std::string weapon) :
	position(move(position)), radius(move(radius)), damage(move(damage)), occlusion(move(occlusion)), power(move(power)),
	source(move(source)), weapon(move(weapon)) {

}

AreaDamageChange::~AreaDamageChange() {

}

const string& AreaDamageChange::GetType() const {
	static const string type = "area_damage";
	return type;
}

void AreaDamageChange::SetPosition(std::string position) {
	this->position = move(position);
}

const std::string& AreaDamageChange::GetPosition() const {
	return position;
}

void AreaDamageChange::SetRadius(std::string radius) {
	this->radius = move(radius);
}

const std::string& AreaDamageChange::GetRadius() const {
	return radius;
}

void AreaDamageChange::SetDamage(std::string damage) {
	this->damage = move(damage);
}

const std::string& AreaDamageChange::GetDamage() const {
	return damage;
}

void AreaDamageChange::SetOcclusion(std::string occlusion) {
	this->occlusion = move(occlusion);
}

const std::string& AreaDamageChange::GetOcclusion() const {
	return occlusion;
}

void AreaDamageChange::SetPower(std::string power) {
	this->power = move(power);
}

const std::string& AreaDamageChange::GetPower() const {
	return power;
}

void AreaDamageChange::SetSource(std::string source) {
	this->source = move(source);
}

const std::string& AreaDamageChange::GetSource() const {
	return source;
}

void AreaDamageChange::SetWeapon(std::string weapon) {
	this->weapon = move(weapon);
}

const std::string& AreaDamageChange::GetWeapon() const {
	return weapon;
}

AddSatietyChange::AddSatietyChange() :
	target(), amount() {

}

AddSatietyChange::AddSatietyChange(std::string target, std::string amount) :
	target(move(target)), amount(move(amount)) {

}

AddSatietyChange::~AddSatietyChange() {

}

const string& AddSatietyChange::GetType() const {
	static const string type = "add_satiety";
	return type;
}

void AddSatietyChange::SetTarget(std::string target) {
	this->target = move(target);
}

const std::string& AddSatietyChange::GetTarget() const {
	return target;
}

void AddSatietyChange::SetAmount(std::string amount) {
	this->amount = move(amount);
}

const std::string& AddSatietyChange::GetAmount() const {
	return amount;
}
