#include "change.h"


using namespace std;

Change::Change() :
	condition() {

}

Change::~Change() {

}

const Expression& Change::GetCondition() const {
	return condition;
}

void Change::SetCondition(const Expression& condition) {
	this->condition = condition;
}

ForRangeChange::ForRangeChange(string var, Expression from, Expression to, Expression step,
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

const Expression& ForRangeChange::GetFrom() const {
	return from;
}

const Expression& ForRangeChange::GetTo() const {
	return to;
}

const Expression& ForRangeChange::GetStep() const {
	return step;
}

const vector<const Change*>& ForRangeChange::GetChanges() const {
	return changes;
}

PlaceHolderChange::PlaceHolderChange() :
	label() {

}

PlaceHolderChange::PlaceHolderChange(Expression label) :
	label(move(label)) {

}

PlaceHolderChange::~PlaceHolderChange() {

}

const string& PlaceHolderChange::GetType() const {
	static const string type = "place_holder";
	return type;
}

void PlaceHolderChange::SetLabel(Expression label) {
	this->label = move(label);
}

const Expression& PlaceHolderChange::GetLabel() const {
	return label;
}

GlobalMessageChange::GlobalMessageChange() :
	message() {

}

GlobalMessageChange::GlobalMessageChange(Expression message) :
	message(move(message)) {

}

GlobalMessageChange::~GlobalMessageChange() {

}

const string& GlobalMessageChange::GetType() const {
	static const string type = "global_message";
	return type;
}

void GlobalMessageChange::SetMessage(Expression message) {
	this->message = move(message);
}

const Expression& GlobalMessageChange::GetMessage() const {
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

SetValueChange::SetValueChange(string variable, Expression value) :
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

void SetValueChange::SetValue(Expression value) {
	this->value = move(value);
}

const Expression& SetValueChange::GetValue() const {
	return value;
}

GlobalSettingChange::GlobalSettingChange() :
	setting(), value() {

}

GlobalSettingChange::GlobalSettingChange(string setting, Expression value) :
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

void GlobalSettingChange::SetValue(Expression value) {
	this->value = move(value);
}

const Expression& GlobalSettingChange::GetValue() const {
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

AddOptionChange::AddOptionChange(Expression name, Expression option) :
	name(move(name)), option(move(option)) {

}

AddOptionChange::~AddOptionChange() {

}

const string& AddOptionChange::GetType() const {
	static const string type = "add_option";
	return type;
}

void AddOptionChange::SetName(Expression name) {
	this->name = move(name);
}

const Expression& AddOptionChange::GetName() const {
	return name;
}

void AddOptionChange::SetOption(Expression option) {
	this->option = move(option);
}

const Expression& AddOptionChange::GetOption() const {
	return option;
}

RemoveOptionChange::RemoveOptionChange() :
	name(), option() {

}

RemoveOptionChange::RemoveOptionChange(Expression name, Expression option) :
	name(move(name)), option(move(option)) {

}

RemoveOptionChange::~RemoveOptionChange() {

}

const string& RemoveOptionChange::GetType() const {
	static const string type = "remove_option";
	return type;
}

void RemoveOptionChange::SetName(Expression name) {
	this->name = move(name);
}

const Expression& RemoveOptionChange::GetName() const {
	return name;
}

void RemoveOptionChange::SetOption(Expression option) {
	this->option = move(option);
}

const Expression& RemoveOptionChange::GetOption() const {
	return option;
}

AddGlobalChange::AddGlobalChange() :
	option() {

}

AddGlobalChange::AddGlobalChange(Expression option) :
	option(move(option)) {

}

AddGlobalChange::~AddGlobalChange() {

}

const string& AddGlobalChange::GetType() const {
	static const string type = "add_global";
	return type;
}

void AddGlobalChange::SetOption(Expression option) {
	this->option = move(option);
}

const Expression& AddGlobalChange::GetOption() const {
	return option;
}

RemoveGlobalChange::RemoveGlobalChange() :
	option() {

}

RemoveGlobalChange::RemoveGlobalChange(Expression option) :
	option(move(option)) {

}

RemoveGlobalChange::~RemoveGlobalChange() {

}

const string& RemoveGlobalChange::GetType() const {
	static const string type = "remove_global";
	return type;
}

void RemoveGlobalChange::SetOption(Expression option) {
	this->option = move(option);
}

const Expression& RemoveGlobalChange::GetOption() const {
	return option;
}

SpawnNpcChange::SpawnNpcChange() :
	avatar(), name(), gender(), birthday(), height(), weight(),
	nick(), deposit(), phone(), home(), jobs(), scheduler() {

}

SpawnNpcChange::SpawnNpcChange(Expression avatar, Expression name, Expression gender, Expression birthday, Expression height, Expression weight,
	Expression nick, Expression deposit, Expression phone, Expression home, vector<Expression> jobs, Expression scheduler) :
	avatar(move(avatar)), name(move(name)), gender(move(gender)), birthday(move(birthday)), height(move(height)), weight(move(weight)),
	nick(move(nick)), deposit(move(deposit)), phone(move(phone)), home(move(home)), jobs(move(jobs)), scheduler(move(scheduler)) {

}

SpawnNpcChange::~SpawnNpcChange() {

}

const string& SpawnNpcChange::GetType() const {
	static const string type = "spawn_npc";
	return type;
}

void SpawnNpcChange::SetAvatar(Expression avatar) {
	this->avatar = move(avatar);
}

const Expression& SpawnNpcChange::GetAvatar() const {
	return avatar;
}

void SpawnNpcChange::SetName(Expression name) {
	this->name = move(name);
}

const Expression& SpawnNpcChange::GetName() const {
	return name;
}

void SpawnNpcChange::SetGender(Expression gender) {
	this->gender = move(gender);
}

const Expression& SpawnNpcChange::GetGender() const {
	return gender;
}

void SpawnNpcChange::SetBirthday(Expression birthday) {
	this->birthday = move(birthday);
}

const Expression& SpawnNpcChange::GetBirthday() const {
	return birthday;
}

void SpawnNpcChange::SetHeight(Expression height) {
	this->height = move(height);
}

const Expression& SpawnNpcChange::GetHeight() const {
	return height;
}

void SpawnNpcChange::SetWeight(Expression weight) {
	this->weight = move(weight);
}

const Expression& SpawnNpcChange::GetWeight() const {
	return weight;
}

void SpawnNpcChange::SetNick(Expression nick) {
	this->nick = move(nick);
}

const Expression& SpawnNpcChange::GetNick() const {
	return nick;
}

void SpawnNpcChange::SetDeposit(Expression deposit) {
	this->deposit = move(deposit);
}

const Expression& SpawnNpcChange::GetDeposit() const {
	return deposit;
}

void SpawnNpcChange::SetPhone(Expression phone) {
	this->phone = move(phone);
}

const Expression& SpawnNpcChange::GetPhone() const {
	return phone;
}

void SpawnNpcChange::SetHome(Expression home) {
	this->home = move(home);
}

const Expression& SpawnNpcChange::GetHome() const {
	return home;
}

void SpawnNpcChange::SetJobs(vector<Expression> jobs) {
	this->jobs = move(jobs);
}

const vector<Expression>& SpawnNpcChange::GetJobs() const {
	return jobs;
}

void SpawnNpcChange::SetScheduler(Expression scheduler) {
	this->scheduler = move(scheduler);
}

const Expression& SpawnNpcChange::GetScheduler() const {
	return scheduler;
}

RemoveNpcChange::RemoveNpcChange() :
	name() {

}

RemoveNpcChange::RemoveNpcChange(Expression name) :
	name(move(name)) {

}

RemoveNpcChange::~RemoveNpcChange() {

}

const string& RemoveNpcChange::GetType() const {
	static const string type = "remove_npc";
	return type;
}

void RemoveNpcChange::SetName(Expression name) {
	this->name = move(name);
}

const Expression& RemoveNpcChange::GetName() const {
	return name;
}

TeleportCitizenChange::TeleportCitizenChange() :
	name(), destination() {

}

TeleportCitizenChange::TeleportCitizenChange(Expression name, Expression destination) :
	name(move(name)), destination(move(destination)) {

}

TeleportCitizenChange::~TeleportCitizenChange() {

}

const string& TeleportCitizenChange::GetType() const {
	static const string type = "teleport_citizen";
	return type;
}

void TeleportCitizenChange::SetName(Expression name) {
	this->name = move(name);
}

const Expression& TeleportCitizenChange::GetName() const {
	return name;
}

void TeleportCitizenChange::SetDestination(Expression destination) {
	this->destination = move(destination);
}

const Expression& TeleportCitizenChange::GetDestination() const {
	return destination;
}

NPCNavigateChange::NPCNavigateChange() :
	name(), destination() {

}

NPCNavigateChange::NPCNavigateChange(Expression name, Expression destination) :
	name(move(name)), destination(move(destination)) {

}

NPCNavigateChange::~NPCNavigateChange() {

}

const string& NPCNavigateChange::GetType() const {
	static const string type = "npc_navigate";
	return type;
}

void NPCNavigateChange::SetName(Expression name) {
	this->name = move(name);
}

const Expression& NPCNavigateChange::GetName() const {
	return name;
}

void NPCNavigateChange::SetDestination(Expression destination) {
	this->destination = move(destination);
}

const Expression& NPCNavigateChange::GetDestination() const {
	return destination;
}

TeleportPlayerChange::TeleportPlayerChange() :
	destination() {

}

TeleportPlayerChange::TeleportPlayerChange(Expression destination) :
	destination(move(destination)) {

}

TeleportPlayerChange::~TeleportPlayerChange() {

}

const string& TeleportPlayerChange::GetType() const {
	static const string type = "teleport_player";
	return type;
}

void TeleportPlayerChange::SetDestination(Expression destination) {
	this->destination = move(destination);
}

const Expression& TeleportPlayerChange::GetDestination() const {
	return destination;
}

OpenShopChange::OpenShopChange() :
	saler() {

}

OpenShopChange::OpenShopChange(Expression saler) :
	saler(move(saler)) {

}

OpenShopChange::~OpenShopChange() {

}

const string& OpenShopChange::GetType() const {
	static const string type = "open_shop";
	return type;
}

void OpenShopChange::SetSaler(Expression saler) {
	this->saler = move(saler);
}

const Expression& OpenShopChange::GetSaler() const {
	return saler;
}

StartPuzzleChange::StartPuzzleChange() :
	puzzle() {

}

StartPuzzleChange::StartPuzzleChange(Expression puzzle) :
	puzzle(move(puzzle)) {

}

StartPuzzleChange::~StartPuzzleChange() {

}

const string& StartPuzzleChange::GetType() const {
	static const string type = "start_puzzle";
	return type;
}

void StartPuzzleChange::SetPuzzle(Expression puzzle) {
	this->puzzle = move(puzzle);
}

const Expression& StartPuzzleChange::GetPuzzle() const {
	return puzzle;
}

EnterVehicleChange::EnterVehicleChange() :
	vehicle() {

}

EnterVehicleChange::EnterVehicleChange(Expression vehicle) :
	vehicle(move(vehicle)) {

}

EnterVehicleChange::~EnterVehicleChange() {

}

const string& EnterVehicleChange::GetType() const {
	static const string type = "enter_vehicle";
	return type;
}

void EnterVehicleChange::SetVehicle(Expression vehicle) {
	this->vehicle = move(vehicle);
}

const Expression& EnterVehicleChange::GetVehicle() const {
	return vehicle;
}

LeaveVehicleChange::LeaveVehicleChange() :
	vehicle() {

}

LeaveVehicleChange::LeaveVehicleChange(Expression vehicle) :
	vehicle(move(vehicle)) {

}

LeaveVehicleChange::~LeaveVehicleChange() {

}

const string& LeaveVehicleChange::GetType() const {
	static const string type = "leave_vehicle";
	return type;
}

void LeaveVehicleChange::SetVehicle(Expression vehicle) {
	this->vehicle = move(vehicle);
}

const Expression& LeaveVehicleChange::GetVehicle() const {
	return vehicle;
}

CreateTimerChange::CreateTimerChange() :
	name(), time(), category(), label() {

}

CreateTimerChange::CreateTimerChange(Expression name, Expression time, Expression category, Expression label) :
	name(move(name)), time(move(time)), category(move(category)), label(move(label)) {

}

CreateTimerChange::~CreateTimerChange() {

}

const string& CreateTimerChange::GetType() const {
	static const string type = "create_timer";
	return type;
}

void CreateTimerChange::SetName(Expression name) {
	this->name = move(name);
}

const Expression& CreateTimerChange::GetName() const {
	return name;
}

void CreateTimerChange::SetTime(Expression time) {
	this->time = move(time);
}

const Expression& CreateTimerChange::GetTime() const {
	return time;
}

void CreateTimerChange::SetCategory(Expression category) {
	this->category = move(category);
}

const Expression& CreateTimerChange::GetCategory() const {
	return category;
}

void CreateTimerChange::SetLabel(Expression label) {
	this->label = move(label);
}

const Expression& CreateTimerChange::GetLabel() const {
	return label;
}

RemoveTimerChange::RemoveTimerChange() :
	name() {

}

RemoveTimerChange::RemoveTimerChange(Expression name) :
	name(move(name)) {

}

RemoveTimerChange::~RemoveTimerChange() {

}

const string& RemoveTimerChange::GetType() const {
	static const string type = "remove_timer";
	return type;
}

void RemoveTimerChange::SetName(Expression name) {
	this->name = move(name);
}

const Expression& RemoveTimerChange::GetName() const {
	return name;
}

LaunchElevatorChange::LaunchElevatorChange() :
	building(), elevator(), command() {

}

LaunchElevatorChange::LaunchElevatorChange(Expression building, Expression elevator, Expression command) :
	building(move(building)), elevator(move(elevator)), command(move(command)) {

}

LaunchElevatorChange::~LaunchElevatorChange() {

}

const string& LaunchElevatorChange::GetType() const {
	static const string type = "launch_elevator";
	return type;
}

void LaunchElevatorChange::SetBuilding(Expression building) {
	this->building = move(building);
}

const Expression& LaunchElevatorChange::GetBuilding() const {
	return building;
}

void LaunchElevatorChange::SetElevator(Expression elevator) {
	this->elevator = move(elevator);
}

const Expression& LaunchElevatorChange::GetElevator() const {
	return elevator;
}

void LaunchElevatorChange::SetCommand(Expression command) {
	this->command = move(command);
}

const Expression& LaunchElevatorChange::GetCommand() const {
	return command;
}

PlayVideoChange::PlayVideoChange() :
	path() {

}

PlayVideoChange::PlayVideoChange(Expression path) :
	path(move(path)) {

}

PlayVideoChange::~PlayVideoChange() {

}

const string& PlayVideoChange::GetType() const {
	static const string type = "play_video";
	return type;
}

void PlayVideoChange::SetPath(Expression path) {
	this->path = move(path);
}

const Expression& PlayVideoChange::GetPath() const {
	return path;
}

PlayBgmChange::PlayBgmChange() :
	bgm(), loop() {

}

PlayBgmChange::PlayBgmChange(Expression bgm, Expression loop) :
	bgm(move(bgm)), loop(move(loop)) {

}

PlayBgmChange::~PlayBgmChange() {

}

const string& PlayBgmChange::GetType() const {
	static const string type = "play_bgm";
	return type;
}

void PlayBgmChange::SetBgm(Expression bgm) {
	this->bgm = move(bgm);
}

const Expression& PlayBgmChange::GetBgm() const {
	return bgm;
}

void PlayBgmChange::SetLoop(Expression loop) {
	this->loop = move(loop);
}

const Expression& PlayBgmChange::GetLoop() const {
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

BankTransactionChange::BankTransactionChange(Expression name, Expression amount) :
	name(move(name)), amount(move(amount)) {

}

BankTransactionChange::~BankTransactionChange() {

}

const string& BankTransactionChange::GetType() const {
	static const string type = "bank_transaction";
	return type;
}

void BankTransactionChange::SetName(Expression name) {
	this->name = move(name);
}

const Expression& BankTransactionChange::GetName() const {
	return name;
}

void BankTransactionChange::SetAmount(Expression amount) {
	this->amount = move(amount);
}

const Expression& BankTransactionChange::GetAmount() const {
	return amount;
}

GiveEstateChange::GiveEstateChange() :
	estate(), name(), force() {

}

GiveEstateChange::GiveEstateChange(Expression estate, Expression name, Expression force) :
	estate(move(estate)), name(move(name)), force(move(force)) {

}

GiveEstateChange::~GiveEstateChange() {

}

const string& GiveEstateChange::GetType() const {
	static const string type = "give_estate";
	return type;
}

void GiveEstateChange::SetEstate(Expression estate) {
	this->estate = move(estate);
}

const Expression& GiveEstateChange::GetEstate() const {
	return estate;
}

void GiveEstateChange::SetName(Expression name) {
	this->name = move(name);
}

const Expression& GiveEstateChange::GetName() const {
	return name;
}

void GiveEstateChange::SetForce(Expression force) {
	this->force = move(force);
}

const Expression& GiveEstateChange::GetForce() const {
	return force;
}

RemoveEstateChange::RemoveEstateChange() :
	estate(), name() {

}

RemoveEstateChange::RemoveEstateChange(Expression estate, Expression name) :
	estate(move(estate)), name(move(name)) {

}

RemoveEstateChange::~RemoveEstateChange() {

}

const string& RemoveEstateChange::GetType() const {
	static const string type = "remove_estate";
	return type;
}

void RemoveEstateChange::SetEstate(Expression estate) {
	this->estate = move(estate);
}

const Expression& RemoveEstateChange::GetEstate() const {
	return estate;
}

void RemoveEstateChange::SetName(Expression name) {
	this->name = move(name);
}

const Expression& RemoveEstateChange::GetName() const {
	return name;
}

GiveVehicleChange::GiveVehicleChange() :
	vehicle(), name(), force() {

}

GiveVehicleChange::GiveVehicleChange(Expression vehicle, Expression name, Expression force) :
	vehicle(move(vehicle)), name(move(name)), force(move(force)) {

}

GiveVehicleChange::~GiveVehicleChange() {

}

const string& GiveVehicleChange::GetType() const {
	static const string type = "give_vehicle";
	return type;
}

void GiveVehicleChange::SetVehicle(Expression vehicle) {
	this->vehicle = move(vehicle);
}

const Expression& GiveVehicleChange::GetVehicle() const {
	return vehicle;
}

void GiveVehicleChange::SetName(Expression name) {
	this->name = move(name);
}

const Expression& GiveVehicleChange::GetName() const {
	return name;
}

void GiveVehicleChange::SetForce(Expression force) {
	this->force = move(force);
}

const Expression& GiveVehicleChange::GetForce() const {
	return force;
}

RemoveVehicleChange::RemoveVehicleChange() :
	vehicle(), name() {

}

RemoveVehicleChange::RemoveVehicleChange(Expression vehicle, Expression name) :
	vehicle(move(vehicle)), name(move(name)) {

}

RemoveVehicleChange::~RemoveVehicleChange() {

}

const string& RemoveVehicleChange::GetType() const {
	static const string type = "remove_vehicle";
	return type;
}

void RemoveVehicleChange::SetVehicle(Expression vehicle) {
	this->vehicle = move(vehicle);
}

const Expression& RemoveVehicleChange::GetVehicle() const {
	return vehicle;
}

void RemoveVehicleChange::SetName(Expression name) {
	this->name = move(name);
}

const Expression& RemoveVehicleChange::GetName() const {
	return name;
}

GiveObjectChange::GiveObjectChange() :
	object(), num() {

}

GiveObjectChange::GiveObjectChange(Expression object, Expression num) :
	object(move(object)), num(move(num)) {

}

GiveObjectChange::~GiveObjectChange() {

}

const string& GiveObjectChange::GetType() const {
	static const string type = "give_object";
	return type;
}

void GiveObjectChange::SetObject(Expression object) {
	this->object = move(object);
}

const Expression& GiveObjectChange::GetObject() const {
	return object;
}

void GiveObjectChange::SetNum(Expression num) {
	this->num = move(num);
}

const Expression& GiveObjectChange::GetNum() const {
	return num;
}

RemoveObjectChange::RemoveObjectChange() :
	object(), num(), force() {

}

RemoveObjectChange::RemoveObjectChange(Expression object, Expression num, Expression force) :
	object(move(object)), num(move(num)), force(move(force)) {

}

RemoveObjectChange::~RemoveObjectChange() {

}

const string& RemoveObjectChange::GetType() const {
	static const string type = "remove_object";
	return type;
}

void RemoveObjectChange::SetObject(Expression object) {
	this->object = move(object);
}

const Expression& RemoveObjectChange::GetObject() const {
	return object;
}

void RemoveObjectChange::SetNum(Expression num) {
	this->num = move(num);
}

const Expression& RemoveObjectChange::GetNum() const {
	return num;
}

void RemoveObjectChange::SetForce(Expression force) {
	this->force = move(force);
}

const Expression& RemoveObjectChange::GetForce() const {
	return force;
}

PlayerInjuredChange::PlayerInjuredChange() :
	wound() {

}

PlayerInjuredChange::PlayerInjuredChange(Expression wound) :
	wound(move(wound)) {

}

PlayerInjuredChange::~PlayerInjuredChange() {

}

const string& PlayerInjuredChange::GetType() const {
	static const string type = "player_injured";
	return type;
}

void PlayerInjuredChange::SetWound(Expression wound) {
	this->wound = move(wound);
}

const Expression& PlayerInjuredChange::GetWound() const {
	return wound;
}

PlayerCuredChange::PlayerCuredChange() :
	wound() {

}

PlayerCuredChange::PlayerCuredChange(Expression wound) :
	wound(move(wound)) {

}

PlayerCuredChange::~PlayerCuredChange() {

}

const string& PlayerCuredChange::GetType() const {
	static const string type = "player_cured";
	return type;
}

void PlayerCuredChange::SetWound(Expression wound) {
	this->wound = move(wound);
}

const Expression& PlayerCuredChange::GetWound() const {
	return wound;
}

PlayerIllChange::PlayerIllChange() :
	illness() {

}

PlayerIllChange::PlayerIllChange(Expression illness) :
	illness(move(illness)) {

}

PlayerIllChange::~PlayerIllChange() {

}

const string& PlayerIllChange::GetType() const {
	static const string type = "player_ill";
	return type;
}

void PlayerIllChange::SetIllness(Expression illness) {
	this->illness = move(illness);
}

const Expression& PlayerIllChange::GetIllness() const {
	return illness;
}

PlayerRecoverChange::PlayerRecoverChange() :
	illness() {

}

PlayerRecoverChange::PlayerRecoverChange(Expression illness) :
	illness(move(illness)) {

}

PlayerRecoverChange::~PlayerRecoverChange() {

}

const string& PlayerRecoverChange::GetType() const {
	static const string type = "player_recover";
	return type;
}

void PlayerRecoverChange::SetIllness(Expression illness) {
	this->illness = move(illness);
}

const Expression& PlayerRecoverChange::GetIllness() const {
	return illness;
}

PlayerSleepChange::PlayerSleepChange() :
	hour() {

}

PlayerSleepChange::PlayerSleepChange(Expression hour) :
	hour(move(hour)) {

}

PlayerSleepChange::~PlayerSleepChange() {

}

const string& PlayerSleepChange::GetType() const {
	static const string type = "player_sleep";
	return type;
}

void PlayerSleepChange::SetHour(Expression hour) {
	this->hour = move(hour);
}

const Expression& PlayerSleepChange::GetHour() const {
	return hour;
}

ChangeTimeChange::ChangeTimeChange() :
	delta() {

}

ChangeTimeChange::ChangeTimeChange(Expression delta) :
	delta(move(delta)) {

}

ChangeTimeChange::~ChangeTimeChange() {

}

const string& ChangeTimeChange::GetType() const {
	static const string type = "change_time";
	return type;
}

void ChangeTimeChange::SetDelta(Expression delta) {
	this->delta = move(delta);
}

const Expression& ChangeTimeChange::GetDelta() const {
	return delta;
}

ChangeCultivationChange::ChangeCultivationChange() :
	method(), level() {

}

ChangeCultivationChange::ChangeCultivationChange(Expression method, Expression level) :
	method(move(method)), level(move(level)) {

}

ChangeCultivationChange::~ChangeCultivationChange() {

}

const string& ChangeCultivationChange::GetType() const {
	static const string type = "change_cultivation";
	return type;
}

void ChangeCultivationChange::SetMethod(Expression method) {
	this->method = move(method);
}

const Expression& ChangeCultivationChange::GetMethod() const {
	return method;
}

void ChangeCultivationChange::SetLevel(Expression level) {
	this->level = move(level);
}

const Expression& ChangeCultivationChange::GetLevel() const {
	return level;
}

ChangeWantedChange::ChangeWantedChange() :
	reason(), level() {

}

ChangeWantedChange::ChangeWantedChange(Expression reason, Expression level) :
	reason(move(reason)), level(move(level)) {

}

ChangeWantedChange::~ChangeWantedChange() {

}

const string& ChangeWantedChange::GetType() const {
	static const string type = "change_wanted";
	return type;
}

void ChangeWantedChange::SetReason(Expression reason) {
	this->reason = move(reason);
}

const Expression& ChangeWantedChange::GetReason() const {
	return reason;
}

void ChangeWantedChange::SetLevel(Expression level) {
	this->level = move(level);
}

const Expression& ChangeWantedChange::GetLevel() const {
	return level;
}

ChangeWeatherChange::ChangeWeatherChange() :
	weather() {

}

ChangeWeatherChange::ChangeWeatherChange(Expression weather) :
	weather(move(weather)) {

}

ChangeWeatherChange::~ChangeWeatherChange() {

}

const string& ChangeWeatherChange::GetType() const {
	static const string type = "change_weather";
	return type;
}

void ChangeWeatherChange::SetWeather(Expression weather) {
	this->weather = move(weather);
}

const Expression& ChangeWeatherChange::GetWeather() const {
	return weather;
}

ChangePolicyChange::ChangePolicyChange() :
	policy() {

}

ChangePolicyChange::ChangePolicyChange(Expression policy) :
	policy(move(policy)) {

}

ChangePolicyChange::~ChangePolicyChange() {

}

const string& ChangePolicyChange::GetType() const {
	static const string type = "change_policy";
	return type;
}

void ChangePolicyChange::SetPolicy(Expression policy) {
	this->policy = move(policy);
}

const Expression& ChangePolicyChange::GetPolicy() const {
	return policy;
}

ChangeControlChange::ChangeControlChange() :
	name() {

}

ChangeControlChange::ChangeControlChange(Expression name) :
	name(move(name)) {

}

ChangeControlChange::~ChangeControlChange() {

}

const string& ChangeControlChange::GetType() const {
	static const string type = "change_control";
	return type;
}

void ChangeControlChange::SetName(Expression name) {
	this->name = move(name);
}

const Expression& ChangeControlChange::GetName() const {
	return name;
}
