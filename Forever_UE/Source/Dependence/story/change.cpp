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

GlobalMessageChange::GlobalMessageChange(std::string message) :
	message(move(message)) {

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

AddOptionChange::AddOptionChange(std::string name, std::string option) :
	name(move(name)), option(move(option)) {

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

RemoveOptionChange::RemoveOptionChange() :
	name(), option() {

}

RemoveOptionChange::RemoveOptionChange(std::string name, std::string option) :
	name(move(name)), option(move(option)) {

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

AddGlobalChange::AddGlobalChange() :
	option() {

}

AddGlobalChange::AddGlobalChange(std::string option) :
	option(move(option)) {

}

AddGlobalChange::~AddGlobalChange() {

}

const string& AddGlobalChange::GetType() const {
	static const string type = "add_global";
	return type;
}

void AddGlobalChange::SetOption(std::string option) {
	this->option = move(option);
}

const std::string& AddGlobalChange::GetOption() const {
	return option;
}

RemoveGlobalChange::RemoveGlobalChange() :
	option() {

}

RemoveGlobalChange::RemoveGlobalChange(std::string option) :
	option(move(option)) {

}

RemoveGlobalChange::~RemoveGlobalChange() {

}

const string& RemoveGlobalChange::GetType() const {
	static const string type = "remove_global";
	return type;
}

void RemoveGlobalChange::SetOption(std::string option) {
	this->option = move(option);
}

const std::string& RemoveGlobalChange::GetOption() const {
	return option;
}

SpawnNpcChange::SpawnNpcChange() :
	avatar(), name(), gender(), birthday(), height(), weight(),
	nick(), deposit(), phone(), home(), jobs(), scheduler() {

}

SpawnNpcChange::SpawnNpcChange(std::string avatar, std::string name, std::string gender, std::string birthday, std::string height, std::string weight,
	std::string nick, std::string deposit, std::string phone, std::string home, vector<std::string> jobs, std::string scheduler) :
	avatar(move(avatar)), name(move(name)), gender(move(gender)), birthday(move(birthday)), height(move(height)), weight(move(weight)),
	nick(move(nick)), deposit(move(deposit)), phone(move(phone)), home(move(home)), jobs(move(jobs)), scheduler(move(scheduler)) {

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

TeleportCitizenChange::TeleportCitizenChange() :
	name(), destination() {

}

TeleportCitizenChange::TeleportCitizenChange(std::string name, std::string destination) :
	name(move(name)), destination(move(destination)) {

}

TeleportCitizenChange::~TeleportCitizenChange() {

}

const string& TeleportCitizenChange::GetType() const {
	static const string type = "teleport_citizen";
	return type;
}

void TeleportCitizenChange::SetName(std::string name) {
	this->name = move(name);
}

const std::string& TeleportCitizenChange::GetName() const {
	return name;
}

void TeleportCitizenChange::SetDestination(std::string destination) {
	this->destination = move(destination);
}

const std::string& TeleportCitizenChange::GetDestination() const {
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
	puzzle() {

}

StartPuzzleChange::StartPuzzleChange(std::string puzzle) :
	puzzle(move(puzzle)) {

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

CreateTimerChange::CreateTimerChange() :
	name(), time(), category(), label() {

}

CreateTimerChange::CreateTimerChange(std::string name, std::string time, std::string category, std::string label) :
	name(move(name)), time(move(time)), category(move(category)), label(move(label)) {

}

CreateTimerChange::~CreateTimerChange() {

}

const string& CreateTimerChange::GetType() const {
	static const string type = "create_timer";
	return type;
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

GiveObjectChange::GiveObjectChange() :
	object(), num() {

}

GiveObjectChange::GiveObjectChange(std::string object, std::string num) :
	object(move(object)), num(move(num)) {

}

GiveObjectChange::~GiveObjectChange() {

}

const string& GiveObjectChange::GetType() const {
	static const string type = "give_object";
	return type;
}

void GiveObjectChange::SetObject(std::string object) {
	this->object = move(object);
}

const std::string& GiveObjectChange::GetObject() const {
	return object;
}

void GiveObjectChange::SetNum(std::string num) {
	this->num = move(num);
}

const std::string& GiveObjectChange::GetNum() const {
	return num;
}

RemoveObjectChange::RemoveObjectChange() :
	object(), num(), force() {

}

RemoveObjectChange::RemoveObjectChange(std::string object, std::string num, std::string force) :
	object(move(object)), num(move(num)), force(move(force)) {

}

RemoveObjectChange::~RemoveObjectChange() {

}

const string& RemoveObjectChange::GetType() const {
	static const string type = "remove_object";
	return type;
}

void RemoveObjectChange::SetObject(std::string object) {
	this->object = move(object);
}

const std::string& RemoveObjectChange::GetObject() const {
	return object;
}

void RemoveObjectChange::SetNum(std::string num) {
	this->num = move(num);
}

const std::string& RemoveObjectChange::GetNum() const {
	return num;
}

void RemoveObjectChange::SetForce(std::string force) {
	this->force = move(force);
}

const std::string& RemoveObjectChange::GetForce() const {
	return force;
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

ChangeCultivationChange::ChangeCultivationChange() :
	method(), level() {

}

ChangeCultivationChange::ChangeCultivationChange(std::string method, std::string level) :
	method(move(method)), level(move(level)) {

}

ChangeCultivationChange::~ChangeCultivationChange() {

}

const string& ChangeCultivationChange::GetType() const {
	static const string type = "change_cultivation";
	return type;
}

void ChangeCultivationChange::SetMethod(std::string method) {
	this->method = move(method);
}

const std::string& ChangeCultivationChange::GetMethod() const {
	return method;
}

void ChangeCultivationChange::SetLevel(std::string level) {
	this->level = move(level);
}

const std::string& ChangeCultivationChange::GetLevel() const {
	return level;
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

ChangePolicyChange::ChangePolicyChange() :
	policy() {

}

ChangePolicyChange::ChangePolicyChange(std::string policy) :
	policy(move(policy)) {

}

ChangePolicyChange::~ChangePolicyChange() {

}

const string& ChangePolicyChange::GetType() const {
	static const string type = "change_policy";
	return type;
}

void ChangePolicyChange::SetPolicy(std::string policy) {
	this->policy = move(policy);
}

const std::string& ChangePolicyChange::GetPolicy() const {
	return policy;
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
