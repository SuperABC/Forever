#include "change.h"

#include <unordered_map>


using namespace std;

namespace {
	// 种类 -> 脚本里的类型名，下标就是ChangeKind的数值，顺序必须和change.h的枚举一致(下面有static_assert查个数)。
	const string kChangeKindNames[] = {
		"for_range",
		"place_holder",
		"global_message",
		"debug_print",
		"game_end",
		"set_value",
		"global_setting",
		"remove_value",
		"deactivate_milestone",
		"add_option",
		"remove_option",
		"spawn_npc",
		"remove_npc",
		"npc_revive",
		"npc_teleport",
		"npc_navigate",
		"assign_task",
		"teleport_player",
		"open_shop",
		"start_puzzle",
		"pay_wage",
		"enter_vehicle",
		"show_route",
		"visualize_navigation",
		"vehicle_command",
		"vehicle_navigate",
		"leave_vehicle",
		"board_vehicle",
		"force_leave",
		"set_door",
		"record_object",
		"register_door",
		"jump_time",
		"create_timer",
		"remove_timer",
		"launch_elevator",
		"play_video",
		"play_bgm",
		"stop_bgm",
		"bank_transaction",
		"give_estate",
		"remove_estate",
		"give_vehicle",
		"remove_vehicle",
		"player_injured",
		"player_cured",
		"player_ill",
		"player_recover",
		"player_sleep",
		"change_time",
		"change_wanted",
		"change_weather",
		"change_control",
		"pick_asset",
		"grant_asset",
		"deduct_asset",
		"drop_asset",
		"spawn_effect",
		"sound_effect",
		"spawn_actor",
		"destroy_asset",
		"single_damage",
		"add_satiety",
		"area_damage",
		"citizen_flee",
	};
	constexpr int kChangeKindCount = static_cast<int>(sizeof(kChangeKindNames) / sizeof(kChangeKindNames[0]));
	static_assert(kChangeKindCount == static_cast<int>(ChangeKind::CitizenFlee) + 1, "kChangeKindNames must list every ChangeKind");
}

const string& ChangeKindName(ChangeKind kind) {
	static const string unknown;
	const int index = static_cast<int>(kind);
	return index >= 0 && index < kChangeKindCount ? kChangeKindNames[index] : unknown;
}

bool FindChangeKind(const string& name, ChangeKind& outKind) {
	static const unordered_map<string, ChangeKind> kinds = [] {
		unordered_map<string, ChangeKind> map;
		for (int i = 0; i < kChangeKindCount; i++) map.emplace(kChangeKindNames[i], static_cast<ChangeKind>(i));
		return map;
	}();
	auto it = kinds.find(name);
	if (it == kinds.end()) return false;
	outKind = it->second;
	return true;
}

Change::Change(ChangeKind kind) :
	kind(kind), condition() {

}

Change::~Change() {

}

const std::string& Change::GetType() const {
	return ChangeKindName(kind);
}

const std::string& Change::GetCondition() const {
	return condition;
}

void Change::SetCondition(const std::string& condition) {
	this->condition = condition;
}

ForRangeChange::ForRangeChange(string var, std::string from, std::string to, std::string step,
	vector<const Change*> changes) :
	Change(Kind), var(move(var)), from(move(from)), to(move(to)), step(move(step)), changes(move(changes)) {

}

ForRangeChange::~ForRangeChange() {
	// 子Change归for_range自己持有(Script::BuildChanges解析"do"时new出来的)。
	for (const Change* change : changes) delete change;
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
	Change(Kind), label() {

}

PlaceHolderChange::PlaceHolderChange(std::string label) :
	Change(Kind), label(move(label)) {

}

PlaceHolderChange::~PlaceHolderChange() {

}

void PlaceHolderChange::SetLabel(std::string label) {
	this->label = move(label);
}

const std::string& PlaceHolderChange::GetLabel() const {
	return label;
}

GlobalMessageChange::GlobalMessageChange() :
	Change(Kind), message() {

}

GlobalMessageChange::GlobalMessageChange(std::string message, std::string category) :
	Change(Kind), message(move(message)), category(move(category)) {

}

GlobalMessageChange::~GlobalMessageChange() {

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
	Change(Kind), message() {

}

DebugPrintChange::DebugPrintChange(std::string message) :
	Change(Kind), message(move(message)) {

}

DebugPrintChange::~DebugPrintChange() {

}

void DebugPrintChange::SetMessage(std::string message) {
	this->message = move(message);
}

const std::string& DebugPrintChange::GetMessage() const {
	return message;
}

GameEndChange::GameEndChange() :
	Change(Kind) {

}

GameEndChange::~GameEndChange() {

}

SetValueChange::SetValueChange() :
	Change(Kind), variable(), value() {

}

SetValueChange::SetValueChange(string variable, std::string value) :
	Change(Kind), variable(move(variable)), value(move(value)) {

}

SetValueChange::~SetValueChange() {

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
	Change(Kind), setting(), value() {

}

GlobalSettingChange::GlobalSettingChange(string setting, std::string value) :
	Change(Kind), setting(move(setting)), value(move(value)) {

}

GlobalSettingChange::~GlobalSettingChange() {

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
	Change(Kind), variable() {

}

RemoveValueChange::RemoveValueChange(string variable) :
	Change(Kind), variable(move(variable)) {

}

RemoveValueChange::~RemoveValueChange() {

}

void RemoveValueChange::SetVariable(string variable) {
	this->variable = move(variable);
}

string RemoveValueChange::GetVariable() const {
	return variable;
}

DeactivateMilestoneChange::DeactivateMilestoneChange() :
	Change(Kind), milestone() {

}

DeactivateMilestoneChange::DeactivateMilestoneChange(string milestone) :
	Change(Kind), milestone(move(milestone)) {

}

DeactivateMilestoneChange::~DeactivateMilestoneChange() {

}

void DeactivateMilestoneChange::SetMilestone(string milestone) {
	this->milestone = move(milestone);
}

string DeactivateMilestoneChange::GetMilestone() const {
	return milestone;
}

AddOptionChange::AddOptionChange() :
	Change(Kind), name(), option() {

}

AddOptionChange::AddOptionChange(std::string name, std::string option, std::string category) :
	Change(Kind), name(move(name)), option(move(option)), category(move(category)) {

}

AddOptionChange::~AddOptionChange() {

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
	Change(Kind), name(), option() {

}

RemoveOptionChange::RemoveOptionChange(std::string name, std::string option, std::string category) :
	Change(Kind), name(move(name)), option(move(option)), category(move(category)) {

}

RemoveOptionChange::~RemoveOptionChange() {

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
	Change(Kind), avatar(), name(), gender(), birthday(), height(), weight(),
	nick(), deposit(), phone(), home(), jobs(), scheduler(), economy() {

}

SpawnNpcChange::SpawnNpcChange(std::string avatar, std::string name, std::string gender, std::string birthday, std::string height, std::string weight,
	std::string nick, std::string deposit, std::string phone, std::string home, vector<std::string> jobs, std::string scheduler,
	std::string economy) :
	Change(Kind), avatar(move(avatar)), name(move(name)), gender(move(gender)), birthday(move(birthday)), height(move(height)), weight(move(weight)),
	nick(move(nick)), deposit(move(deposit)), phone(move(phone)), home(move(home)), jobs(move(jobs)), scheduler(move(scheduler)), economy(move(economy)) {

}

SpawnNpcChange::~SpawnNpcChange() {

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
	Change(Kind), name() {

}

RemoveNpcChange::RemoveNpcChange(std::string name) :
	Change(Kind), name(move(name)) {

}

RemoveNpcChange::~RemoveNpcChange() {

}

void RemoveNpcChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& RemoveNpcChange::GetName() const {
	return name;
}

NPCReviveChange::NPCReviveChange() :
	Change(Kind), name(), health() {

}

NPCReviveChange::NPCReviveChange(std::string name, std::string health) :
	Change(Kind), name(move(name)), health(move(health)) {

}

NPCReviveChange::~NPCReviveChange() {

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
	Change(Kind), name(), destination() {

}

NPCTeleportChange::NPCTeleportChange(std::string name, std::string destination) :
	Change(Kind), name(move(name)), destination(move(destination)) {

}

NPCTeleportChange::~NPCTeleportChange() {

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
	Change(Kind), name(), destination(), speed() {

}

NPCNavigateChange::NPCNavigateChange(string name, string destination, string speed) :
	Change(Kind), name(move(name)), destination(move(destination)), speed(move(speed)) {

}

NPCNavigateChange::~NPCNavigateChange() {

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

const string& NPCNavigateChange::GetSpeed() const {
	return speed;
}

AssignTaskChange::AssignTaskChange() :
	Change(Kind), task(), destination(), vehicles(), worker(), speed() {

}

AssignTaskChange::AssignTaskChange(string task, string destination, vector<string> vehicles, string worker, string speed) :
	Change(Kind), task(move(task)), destination(move(destination)), vehicles(move(vehicles)), worker(move(worker)), speed(move(speed)) {

}

AssignTaskChange::~AssignTaskChange() {

}

const string& AssignTaskChange::GetTask() const {
	return task;
}

const string& AssignTaskChange::GetDestination() const {
	return destination;
}

const vector<string>& AssignTaskChange::GetVehicles() const {
	return vehicles;
}

const string& AssignTaskChange::GetWorker() const {
	return worker;
}

const string& AssignTaskChange::GetSpeed() const {
	return speed;
}

TeleportPlayerChange::TeleportPlayerChange() :
	Change(Kind), destination() {

}

TeleportPlayerChange::TeleportPlayerChange(std::string destination) :
	Change(Kind), destination(move(destination)) {

}

TeleportPlayerChange::~TeleportPlayerChange() {

}

void TeleportPlayerChange::SetDestination(std::string destination) {
	this->destination = move(destination);
}

const std::string& TeleportPlayerChange::GetDestination() const {
	return destination;
}

OpenShopChange::OpenShopChange() :
	Change(Kind), saler() {

}

OpenShopChange::OpenShopChange(std::string saler) :
	Change(Kind), saler(move(saler)) {

}

OpenShopChange::~OpenShopChange() {

}

void OpenShopChange::SetSaler(std::string saler) {
	this->saler = move(saler);
}

const std::string& OpenShopChange::GetSaler() const {
	return saler;
}

StartPuzzleChange::StartPuzzleChange() :
	Change(Kind), puzzle(),
	param() {

}

StartPuzzleChange::StartPuzzleChange(std::string puzzle) :
	Change(Kind), puzzle(move(puzzle)),
	param() {

}

StartPuzzleChange::StartPuzzleChange(std::string puzzle, std::string param) :
	Change(Kind), puzzle(move(puzzle)),
	param(move(param)) {

}

StartPuzzleChange::~StartPuzzleChange() {

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
	Change(Kind), amount() {

}

PayWageChange::PayWageChange(std::string amount) :
	Change(Kind), amount(move(amount)) {

}

PayWageChange::~PayWageChange() {

}

void PayWageChange::SetAmount(std::string amount) {
	this->amount = move(amount);
}

const std::string& PayWageChange::GetAmount() const {
	return amount;
}

EnterVehicleChange::EnterVehicleChange() :
	Change(Kind), vehicle() {

}

EnterVehicleChange::EnterVehicleChange(std::string vehicle) :
	Change(Kind), vehicle(move(vehicle)) {

}

EnterVehicleChange::~EnterVehicleChange() {

}

void EnterVehicleChange::SetVehicle(std::string vehicle) {
	this->vehicle = move(vehicle);
}

const std::string& EnterVehicleChange::GetVehicle() const {
	return vehicle;
}

ShowRouteChange::ShowRouteChange() :
	Change(Kind), target() {

}

ShowRouteChange::ShowRouteChange(std::string target) :
	Change(Kind), target(move(target)) {

}

ShowRouteChange::~ShowRouteChange() {

}

const std::string& ShowRouteChange::GetTarget() const {
	return target;
}

VisualizeNavigationChange::VisualizeNavigationChange() :
	Change(Kind), address() {

}

VisualizeNavigationChange::VisualizeNavigationChange(std::string address) :
	Change(Kind), address(move(address)) {

}

VisualizeNavigationChange::~VisualizeNavigationChange() {

}

void VisualizeNavigationChange::SetAddress(std::string address) {
	this->address = move(address);
}

const std::string& VisualizeNavigationChange::GetAddress() const {
	return address;
}

VehicleCommandChange::VehicleCommandChange() :
	Change(Kind), vehicle(), command() {

}

VehicleCommandChange::VehicleCommandChange(std::string vehicle, std::string command) :
	Change(Kind), vehicle(move(vehicle)), command(move(command)) {

}

VehicleCommandChange::~VehicleCommandChange() {

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
	Change(Kind), vehicle(), destination() {

}

VehicleNavigateChange::VehicleNavigateChange(std::string vehicle, std::string destination) :
	Change(Kind), vehicle(move(vehicle)), destination(move(destination)) {

}

VehicleNavigateChange::~VehicleNavigateChange() {

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
	Change(Kind), vehicle() {

}

LeaveVehicleChange::LeaveVehicleChange(std::string vehicle) :
	Change(Kind), vehicle(move(vehicle)) {

}

LeaveVehicleChange::~LeaveVehicleChange() {

}

void LeaveVehicleChange::SetVehicle(std::string vehicle) {
	this->vehicle = move(vehicle);
}

const std::string& LeaveVehicleChange::GetVehicle() const {
	return vehicle;
}

BoardVehicleChange::BoardVehicleChange() :
	Change(Kind), vehicle() {

}

BoardVehicleChange::BoardVehicleChange(std::string vehicle) :
	Change(Kind), vehicle(move(vehicle)) {

}

BoardVehicleChange::~BoardVehicleChange() {

}

void BoardVehicleChange::SetVehicle(std::string vehicle) {
	this->vehicle = move(vehicle);
}

const std::string& BoardVehicleChange::GetVehicle() const {
	return vehicle;
}

ForceLeaveChange::ForceLeaveChange() :
	Change(Kind), vehicle() {

}

ForceLeaveChange::ForceLeaveChange(std::string vehicle) :
	Change(Kind), vehicle(move(vehicle)) {

}

ForceLeaveChange::~ForceLeaveChange() {

}

void ForceLeaveChange::SetVehicle(std::string vehicle) {
	this->vehicle = move(vehicle);
}

const std::string& ForceLeaveChange::GetVehicle() const {
	return vehicle;
}

SetDoorChange::SetDoorChange() :
	Change(Kind), name(), access() {

}

SetDoorChange::SetDoorChange(std::string name, std::string access) :
	Change(Kind), name(move(name)), access(move(access)) {

}

SetDoorChange::~SetDoorChange() {

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
	Change(Kind), object(), kind(), policy(), variable() {

}

RecordObjectChange::RecordObjectChange(std::string object, std::string kind, std::string policy, std::string variable) :
	Change(Kind), object(move(object)), kind(move(kind)), policy(move(policy)), variable(move(variable)) {

}

RecordObjectChange::~RecordObjectChange() {

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
	Change(Kind), name(), citizen(), pass() {

}

RegisterDoorChange::RegisterDoorChange(std::string name, std::string citizen, std::string pass) :
	Change(Kind), name(move(name)), citizen(move(citizen)), pass(move(pass)) {

}

RegisterDoorChange::~RegisterDoorChange() {

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
	Change(Kind), time(), mode() {

}

JumpTimeChange::JumpTimeChange(std::string time, std::string mode) :
	Change(Kind), time(move(time)), mode(move(mode)) {

}

JumpTimeChange::~JumpTimeChange() {

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
	Change(Kind), name(), time(), category(), label(), mode() {

}

CreateTimerChange::CreateTimerChange(std::string name, std::string time, std::string category, std::string label,
	std::string mode) :
	Change(Kind), name(move(name)), time(move(time)), category(move(category)), label(move(label)), mode(move(mode)) {

}

CreateTimerChange::~CreateTimerChange() {

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
	Change(Kind), name() {

}

RemoveTimerChange::RemoveTimerChange(std::string name) :
	Change(Kind), name(move(name)) {

}

RemoveTimerChange::~RemoveTimerChange() {

}

void RemoveTimerChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& RemoveTimerChange::GetName() const {
	return name;
}

LaunchElevatorChange::LaunchElevatorChange() :
	Change(Kind), building(), elevator(), command() {

}

LaunchElevatorChange::LaunchElevatorChange(std::string building, std::string elevator, std::string command) :
	Change(Kind), building(move(building)), elevator(move(elevator)), command(move(command)) {

}

LaunchElevatorChange::~LaunchElevatorChange() {

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
	Change(Kind), path() {

}

PlayVideoChange::PlayVideoChange(std::string path) :
	Change(Kind), path(move(path)) {

}

PlayVideoChange::~PlayVideoChange() {

}

void PlayVideoChange::SetPath(std::string path) {
	this->path = move(path);
}

const std::string& PlayVideoChange::GetPath() const {
	return path;
}

PlayBgmChange::PlayBgmChange() :
	Change(Kind), bgm(), loop() {

}

PlayBgmChange::PlayBgmChange(std::string bgm, std::string loop) :
	Change(Kind), bgm(move(bgm)), loop(move(loop)) {

}

PlayBgmChange::~PlayBgmChange() {

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

StopBgmChange::StopBgmChange() :
	Change(Kind) {

}

StopBgmChange::~StopBgmChange() {

}

BankTransactionChange::BankTransactionChange() :
	Change(Kind), name(), amount() {

}

BankTransactionChange::BankTransactionChange(std::string name, std::string amount) :
	Change(Kind), name(move(name)), amount(move(amount)) {

}

BankTransactionChange::~BankTransactionChange() {

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
	Change(Kind), estate(), name(), force() {

}

GiveEstateChange::GiveEstateChange(std::string estate, std::string name, std::string force) :
	Change(Kind), estate(move(estate)), name(move(name)), force(move(force)) {

}

GiveEstateChange::~GiveEstateChange() {

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
	Change(Kind), estate(), name() {

}

RemoveEstateChange::RemoveEstateChange(std::string estate, std::string name) :
	Change(Kind), estate(move(estate)), name(move(name)) {

}

RemoveEstateChange::~RemoveEstateChange() {

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
	Change(Kind), vehicle(), name(), force() {

}

GiveVehicleChange::GiveVehicleChange(std::string vehicle, std::string name, std::string force) :
	Change(Kind), vehicle(move(vehicle)), name(move(name)), force(move(force)) {

}

GiveVehicleChange::~GiveVehicleChange() {

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
	Change(Kind), vehicle(), name() {

}

RemoveVehicleChange::RemoveVehicleChange(std::string vehicle, std::string name) :
	Change(Kind), vehicle(move(vehicle)), name(move(name)) {

}

RemoveVehicleChange::~RemoveVehicleChange() {

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
	Change(Kind), wound() {

}

PlayerInjuredChange::PlayerInjuredChange(std::string wound) :
	Change(Kind), wound(move(wound)) {

}

PlayerInjuredChange::~PlayerInjuredChange() {

}

void PlayerInjuredChange::SetWound(std::string wound) {
	this->wound = move(wound);
}

const std::string& PlayerInjuredChange::GetWound() const {
	return wound;
}

PlayerCuredChange::PlayerCuredChange() :
	Change(Kind), wound() {

}

PlayerCuredChange::PlayerCuredChange(std::string wound) :
	Change(Kind), wound(move(wound)) {

}

PlayerCuredChange::~PlayerCuredChange() {

}

void PlayerCuredChange::SetWound(std::string wound) {
	this->wound = move(wound);
}

const std::string& PlayerCuredChange::GetWound() const {
	return wound;
}

PlayerIllChange::PlayerIllChange() :
	Change(Kind), illness() {

}

PlayerIllChange::PlayerIllChange(std::string illness) :
	Change(Kind), illness(move(illness)) {

}

PlayerIllChange::~PlayerIllChange() {

}

void PlayerIllChange::SetIllness(std::string illness) {
	this->illness = move(illness);
}

const std::string& PlayerIllChange::GetIllness() const {
	return illness;
}

PlayerRecoverChange::PlayerRecoverChange() :
	Change(Kind), illness() {

}

PlayerRecoverChange::PlayerRecoverChange(std::string illness) :
	Change(Kind), illness(move(illness)) {

}

PlayerRecoverChange::~PlayerRecoverChange() {

}

void PlayerRecoverChange::SetIllness(std::string illness) {
	this->illness = move(illness);
}

const std::string& PlayerRecoverChange::GetIllness() const {
	return illness;
}

PlayerSleepChange::PlayerSleepChange() :
	Change(Kind), hour() {

}

PlayerSleepChange::PlayerSleepChange(std::string hour) :
	Change(Kind), hour(move(hour)) {

}

PlayerSleepChange::~PlayerSleepChange() {

}

void PlayerSleepChange::SetHour(std::string hour) {
	this->hour = move(hour);
}

const std::string& PlayerSleepChange::GetHour() const {
	return hour;
}

ChangeTimeChange::ChangeTimeChange() :
	Change(Kind), delta() {

}

ChangeTimeChange::ChangeTimeChange(std::string delta) :
	Change(Kind), delta(move(delta)) {

}

ChangeTimeChange::~ChangeTimeChange() {

}

void ChangeTimeChange::SetDelta(std::string delta) {
	this->delta = move(delta);
}

const std::string& ChangeTimeChange::GetDelta() const {
	return delta;
}

ChangeWantedChange::ChangeWantedChange() :
	Change(Kind), reason(), level() {

}

ChangeWantedChange::ChangeWantedChange(std::string reason, std::string level) :
	Change(Kind), reason(move(reason)), level(move(level)) {

}

ChangeWantedChange::~ChangeWantedChange() {

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
	Change(Kind), weather() {

}

ChangeWeatherChange::ChangeWeatherChange(std::string weather) :
	Change(Kind), weather(move(weather)) {

}

ChangeWeatherChange::~ChangeWeatherChange() {

}

void ChangeWeatherChange::SetWeather(std::string weather) {
	this->weather = move(weather);
}

const std::string& ChangeWeatherChange::GetWeather() const {
	return weather;
}

ChangeControlChange::ChangeControlChange() :
	Change(Kind), name() {

}

ChangeControlChange::ChangeControlChange(std::string name) :
	Change(Kind), name(move(name)) {

}

ChangeControlChange::~ChangeControlChange() {

}

void ChangeControlChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& ChangeControlChange::GetName() const {
	return name;
}

PickAssetChange::PickAssetChange() :
	Change(Kind), asset() {

}

PickAssetChange::PickAssetChange(std::string asset) :
	Change(Kind), asset(move(asset)) {

}

PickAssetChange::~PickAssetChange() {

}

void PickAssetChange::SetAsset(std::string asset) {
	this->asset = move(asset);
}

const std::string& PickAssetChange::GetAsset() const {
	return asset;
}

GrantAssetChange::GrantAssetChange() :
	Change(Kind), type(), count(), slot() {

}

GrantAssetChange::GrantAssetChange(std::string type, std::string count, std::string slot) :
	Change(Kind), type(move(type)), count(move(count)), slot(move(slot)) {

}

GrantAssetChange::~GrantAssetChange() {

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

void GrantAssetChange::SetSlot(std::string slot) {
	this->slot = move(slot);
}

const std::string& GrantAssetChange::GetSlot() const {
	return slot;
}

DeductAssetChange::DeductAssetChange() :
	Change(Kind), type(), count() {

}

DeductAssetChange::DeductAssetChange(std::string type, std::string count) :
	Change(Kind), type(move(type)), count(move(count)) {

}

DeductAssetChange::~DeductAssetChange() {

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
	Change(Kind), type(), count() {

}

DropAssetChange::DropAssetChange(std::string type, std::string count) :
	Change(Kind), type(move(type)), count(move(count)) {

}

DropAssetChange::~DropAssetChange() {

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
	Change(Kind), effect(), scale(), asset() {

}

SpawnEffectChange::SpawnEffectChange(std::string effect, std::string scale, std::string asset, std::string position,
	std::string normal) :
	Change(Kind), effect(move(effect)), scale(move(scale)), asset(move(asset)), position(move(position)), normal(move(normal)) {

}

SpawnEffectChange::~SpawnEffectChange() {

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

SoundEffectChange::SoundEffectChange() :
	Change(Kind), sound(), position(), asset(), volume(), pitch() {

}

SoundEffectChange::SoundEffectChange(std::string sound, std::string position, std::string asset, std::string volume,
	std::string pitch) :
	Change(Kind), sound(move(sound)), position(move(position)), asset(move(asset)), volume(move(volume)), pitch(move(pitch)) {

}

SoundEffectChange::~SoundEffectChange() {

}

void SoundEffectChange::SetSound(std::string sound) {
	this->sound = move(sound);
}

const std::string& SoundEffectChange::GetSound() const {
	return sound;
}

void SoundEffectChange::SetPosition(std::string position) {
	this->position = move(position);
}

const std::string& SoundEffectChange::GetPosition() const {
	return position;
}

void SoundEffectChange::SetAsset(std::string asset) {
	this->asset = move(asset);
}

const std::string& SoundEffectChange::GetAsset() const {
	return asset;
}

void SoundEffectChange::SetVolume(std::string volume) {
	this->volume = move(volume);
}

const std::string& SoundEffectChange::GetVolume() const {
	return volume;
}

void SoundEffectChange::SetPitch(std::string pitch) {
	this->pitch = move(pitch);
}

const std::string& SoundEffectChange::GetPitch() const {
	return pitch;
}

SpawnActorChange::SpawnActorChange() :
	Change(Kind), actor(), position(), direction(), scale() {

}

SpawnActorChange::SpawnActorChange(std::string actor, std::string position, std::string direction, std::string scale) :
	Change(Kind), actor(move(actor)), position(move(position)), direction(move(direction)), scale(move(scale)) {

}

SpawnActorChange::~SpawnActorChange() {

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
	Change(Kind), asset() {

}

DestroyAssetChange::DestroyAssetChange(std::string asset) :
	Change(Kind), asset(move(asset)) {

}

DestroyAssetChange::~DestroyAssetChange() {

}

void DestroyAssetChange::SetAsset(std::string asset) {
	this->asset = move(asset);
}

const std::string& DestroyAssetChange::GetAsset() const {
	return asset;
}

SingleDamageChange::SingleDamageChange() :
	Change(Kind), target(), damage(), source(), weapon(), part() {

}

SingleDamageChange::SingleDamageChange(std::string target, std::string damage, std::string source, std::string weapon, std::string part) :
	Change(Kind), target(move(target)), damage(move(damage)), source(move(source)), weapon(move(weapon)), part(move(part)) {

}

SingleDamageChange::~SingleDamageChange() {

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
	Change(Kind), position(), radius(), damage(), occlusion(), power(), source(), weapon() {

}

AreaDamageChange::AreaDamageChange(std::string position, std::string radius, std::string damage, std::string occlusion,
	std::string power, std::string source, std::string weapon) :
	Change(Kind), position(move(position)), radius(move(radius)), damage(move(damage)), occlusion(move(occlusion)), power(move(power)),
	source(move(source)), weapon(move(weapon)) {

}

AreaDamageChange::~AreaDamageChange() {

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
	Change(Kind), target(), amount() {

}

AddSatietyChange::AddSatietyChange(std::string target, std::string amount) :
	Change(Kind), target(move(target)), amount(move(amount)) {

}

AddSatietyChange::~AddSatietyChange() {

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

CitizenFleeChange::CitizenFleeChange() :
	Change(Kind), name(), from() {

}

CitizenFleeChange::CitizenFleeChange(string name, string from) :
	Change(Kind), name(move(name)), from(move(from)) {

}

CitizenFleeChange::~CitizenFleeChange() {

}

const string& CitizenFleeChange::GetName() const {
	return name;
}

const string& CitizenFleeChange::GetFrom() const {
	return from;
}
