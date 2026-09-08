#include "change.h"


using namespace std;

Change::Change() :
	condition() {

}

Change::~Change() {

}

const Condition& Change::GetCondition() const {
	return condition;
}

void Change::SetCondition(const Condition& condition) {
	this->condition = condition;
}

ForRangeChange::ForRangeChange(string var, string from, string to, string step, vector<Change*> changes)
	: var(var), from(from), to(to), step(step), changes(changes.begin(), changes.end()) {}

ForRangeChange::~ForRangeChange() {
	for (auto c : changes) delete c;
}

const string& ForRangeChange::GetType() const { static const string type = "for_range"; return type; }
string ForRangeChange::GetVar() const { return var; }
string ForRangeChange::GetFrom() const { return from; }
string ForRangeChange::GetTo() const { return to; }
string ForRangeChange::GetStep() const { return step; }
const vector<const Change*>& ForRangeChange::GetChanges() const { return changes; }

PlaceHolderChange::PlaceHolderChange() :
	label() {

}

PlaceHolderChange::PlaceHolderChange(string label) :
	label(label) {

}

PlaceHolderChange::~PlaceHolderChange() {

}

const string& PlaceHolderChange::GetType() const {
	static const string type = "place_holder";
	return type;
}

void PlaceHolderChange::SetLabel(string label) {
	this->label = label;
}

string PlaceHolderChange::GetLabel() const {
	return label;
}

GlobalMessageChange::GlobalMessageChange() :
	message() {

}

GlobalMessageChange::GlobalMessageChange(string message) :
	message(message) {

}

GlobalMessageChange::~GlobalMessageChange() {

}

const string& GlobalMessageChange::GetType() const {
	static const string type = "global_message";
	return type;
}

void GlobalMessageChange::SetMessage(string message) {
	this->message = message;
}

string GlobalMessageChange::GetMessage() const {
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
	variable(),
	value() {

}

SetValueChange::SetValueChange(string variable, string value) :
	variable(variable), value(value) {

}

SetValueChange::~SetValueChange() {

}

const string& SetValueChange::GetType() const {
	static const string type = "set_value";
	return type;
}

void SetValueChange::SetVariable(string variable) {
	this->variable = variable;
}

string SetValueChange::GetVariable() const {
	return variable;
}

void SetValueChange::SetValue(string value) {
	this->value = value;
}

string SetValueChange::GetValue() const {
	return value;
}

GlobalSettingChange::GlobalSettingChange() :
	setting(),
	value() {

}

GlobalSettingChange::GlobalSettingChange(string setting, string value) :
	setting(setting),
	value(value) {

}

GlobalSettingChange::~GlobalSettingChange() {

}

const string& GlobalSettingChange::GetType() const {
	static const string type = "global_setting";
	return type;
}

void GlobalSettingChange::SetSetting(string setting) {
	this->setting = setting;
}

string GlobalSettingChange::GetSetting() const {
	return setting;
}

void GlobalSettingChange::SetValue(string value) {
	this->value = value;
}

string GlobalSettingChange::GetValue() const {
	return value;
}

RemoveValueChange::RemoveValueChange() :
	variable() {

}

RemoveValueChange::RemoveValueChange(string variable) :
	variable(variable) {

}

RemoveValueChange::~RemoveValueChange() {

}

const string& RemoveValueChange::GetType() const {
	static const string type = "remove_value";
	return type;
}

void RemoveValueChange::SetVariable(string variable) {
	this->variable = variable;
}

string RemoveValueChange::GetVariable() const {
	return variable;
}

DeactivateMilestoneChange::DeactivateMilestoneChange() :
	milestone() {

}

DeactivateMilestoneChange::DeactivateMilestoneChange(string milestone) :
	milestone(milestone) {

}

DeactivateMilestoneChange::~DeactivateMilestoneChange() {

}

const string& DeactivateMilestoneChange::GetType() const {
	static const string type = "deactivate_milestone";
	return type;
}

void DeactivateMilestoneChange::SetMilestone(string milestone) {
	this->milestone = milestone;
}

string DeactivateMilestoneChange::GetMilestone() const {
	return milestone;
}

AddOptionChange::AddOptionChange() :
	name(),
	option() {

}

AddOptionChange::AddOptionChange(string name, string option) :
	name(name), option(option) {

}

AddOptionChange::~AddOptionChange() {

}

const string& AddOptionChange::GetType() const {
	static const string type = "add_option";
	return type;
}

void AddOptionChange::SetName(string name) {
	this->name = name;
}

string AddOptionChange::GetName() const {
	return name;
}

void AddOptionChange::SetOption(string option) {
	this->option = option;
}

string AddOptionChange::GetOption() const {
	return option;
}

RemoveOptionChange::RemoveOptionChange() :
	name(),
	option() {

}

RemoveOptionChange::RemoveOptionChange(string name, string option) :
	name(name), option(option) {

}

RemoveOptionChange::~RemoveOptionChange() {

}

const string& RemoveOptionChange::GetType() const {
	static const string type = "remove_option";
	return type;
}

void RemoveOptionChange::SetName(string name) {
	this->name = name;
}

string RemoveOptionChange::GetName() const {
	return name;
}

void RemoveOptionChange::SetOption(string option) {
	this->option = option;
}

string RemoveOptionChange::GetOption() const {
	return option;
}

AddGlobalChange::AddGlobalChange() :
	option() {

}

AddGlobalChange::AddGlobalChange(string option) :
	option(option) {

}

AddGlobalChange::~AddGlobalChange() {

}

const string& AddGlobalChange::GetType() const {
	static const string type = "add_global";
	return type;
}

void AddGlobalChange::SetOption(string option) {
	this->option = option;
}

string AddGlobalChange::GetOption() const {
	return option;
}

RemoveGlobalChange::RemoveGlobalChange() :
	option() {

}

RemoveGlobalChange::RemoveGlobalChange(string option) :
	option(option) {

}

RemoveGlobalChange::~RemoveGlobalChange() {

}

const string& RemoveGlobalChange::GetType() const {
	static const string type = "remove_global";
	return type;
}

void RemoveGlobalChange::SetOption(string option) {
	this->option = option;
}

string RemoveGlobalChange::GetOption() const {
	return option;
}

SpawnNpcChange::SpawnNpcChange() :
	avatar(),
	name(),
	gender(),
	birthday(),
	height(0.f),
	weight(0.f),
	nick(),
	deposit(0),
	phone(0),
	home(),
	jobs(),
	scheduler() {

}

SpawnNpcChange::SpawnNpcChange(string avatar, string name, string gender, string birthday, float height, float weight,
	string nick, int deposit, int phone, string home, vector<string> jobs, string scheduler) :
	avatar(avatar), name(name), gender(gender), birthday(birthday), height(height), weight(weight),
	nick(nick), deposit(deposit), phone(phone), home(home), jobs(jobs), scheduler(scheduler) {

}

SpawnNpcChange::~SpawnNpcChange() {

}

const string& SpawnNpcChange::GetType() const {
	static const string type = "spawn_npc";
	return type;
}

void SpawnNpcChange::SetAvatar(string avatar) {
	this->avatar = avatar;
}

string SpawnNpcChange::GetAvatar() const {
	return avatar;
}

void SpawnNpcChange::SetName(string name) {
	this->name = name;
}

string SpawnNpcChange::GetName() const {
	return name;
}

void SpawnNpcChange::SetGender(string gender) {
	this->gender = gender;
}

string SpawnNpcChange::GetGender() const {
	return gender;
}

void SpawnNpcChange::SetBirthday(string birthday) {
	this->birthday = birthday;
}

string SpawnNpcChange::GetBirthday() const {
	return birthday;
}

void SpawnNpcChange::SetHeight(float height) {
	this->height = height;
}

float SpawnNpcChange::GetHeight() const {
	return height;
}

void SpawnNpcChange::SetWeight(float weight) {
	this->weight = weight;
}

float SpawnNpcChange::GetWeight() const {
	return weight;
}

void SpawnNpcChange::SetNick(string nick) {
	this->nick = nick;
}

string SpawnNpcChange::GetNick() const {
	return nick;
}

void SpawnNpcChange::SetDeposit(int deposit) {
	this->deposit = deposit;
}

int SpawnNpcChange::GetDeposit() const {
	return deposit;
}

void SpawnNpcChange::SetPhone(int phone) {
	this->phone = phone;
}

int SpawnNpcChange::GetPhone() const {
	return phone;
}

void SpawnNpcChange::SetHome(string home) {
	this->home = home;
}

string SpawnNpcChange::GetHome() const {
	return home;
}

void SpawnNpcChange::SetJobs(vector<string> jobs) {
	this->jobs = jobs;
}

vector<string> SpawnNpcChange::GetJobs() const {
	return jobs;
}

void SpawnNpcChange::SetScheduler(string scheduler) {
	this->scheduler = scheduler;
}

string SpawnNpcChange::GetScheduler() const {
	return scheduler;
}

RemoveNpcChange::RemoveNpcChange() :
	name() {

}

RemoveNpcChange::RemoveNpcChange(string name) :
	name(name) {

}

RemoveNpcChange::~RemoveNpcChange() {

}

const string& RemoveNpcChange::GetType() const {
	static const string type = "remove_npc";
	return type;
}

void RemoveNpcChange::SetName(string name) {
	this->name = name;
}

string RemoveNpcChange::GetName() const {
	return name;
}

TeleportCitizenChange::TeleportCitizenChange() :
	name(),
	destination() {

}

TeleportCitizenChange::TeleportCitizenChange(string name, string destination) :
	name(name), destination(destination) {

}

TeleportCitizenChange::~TeleportCitizenChange() {

}

const string& TeleportCitizenChange::GetType() const {
	static const string type = "teleport_citizen";
	return type;
}

void TeleportCitizenChange::SetName(string name) {
	this->name = name;
}

string TeleportCitizenChange::GetName() const {
	return name;
}

void TeleportCitizenChange::SetDestination(string destination) {
	this->destination = destination;
}

string TeleportCitizenChange::GetDestination() const {
	return destination;
}

NPCNavigateChange::NPCNavigateChange() :
	name(),
	destination() {

}

NPCNavigateChange::NPCNavigateChange(string name, string destination) :
	name(name), destination(destination) {

}

NPCNavigateChange::~NPCNavigateChange() {

}

const string& NPCNavigateChange::GetType() const {
	static const string type = "npc_navigate";
	return type;
}

void NPCNavigateChange::SetName(string name) {
	this->name = name;
}

string NPCNavigateChange::GetName() const {
	return name;
}

void NPCNavigateChange::SetDestination(string destination) {
	this->destination = destination;
}

string NPCNavigateChange::GetDestination() const {
	return destination;
}

TeleportPlayerChange::TeleportPlayerChange() :
	destination() {

}

TeleportPlayerChange::TeleportPlayerChange(string destination) :
	destination(destination) {

}

TeleportPlayerChange::~TeleportPlayerChange() {

}

const string& TeleportPlayerChange::GetType() const {
	static const string type = "teleport_player";
	return type;
}

void TeleportPlayerChange::SetDestination(string destination) {
	this->destination = destination;
}

string TeleportPlayerChange::GetDestination() const {
	return destination;
}

OpenShopChange::OpenShopChange() :
	saler() {

}

OpenShopChange::OpenShopChange(string saler) :
	saler(saler) {

}

OpenShopChange::~OpenShopChange() {

}

const string& OpenShopChange::GetType() const {
	static const string type = "open_shop";
	return type;
}

void OpenShopChange::SetSaler(string saler) {
	this->saler = saler;
}

string OpenShopChange::GetSaler() const {
	return saler;
}

StartPuzzleChange::StartPuzzleChange() :
	puzzle() {

}

StartPuzzleChange::StartPuzzleChange(string puzzle) :
	puzzle(puzzle) {

}

StartPuzzleChange::~StartPuzzleChange() {

}

const string& StartPuzzleChange::GetType() const {
	static const string type = "start_puzzle";
	return type;
}

void StartPuzzleChange::SetPuzzle(string puzzle) {
	this->puzzle = puzzle;
}

string StartPuzzleChange::GetPuzzle() const {
	return puzzle;
}

EnterVehicleChange::EnterVehicleChange() :
	vehicle() {

}

EnterVehicleChange::EnterVehicleChange(string vehicle) :
	vehicle(vehicle) {

}

EnterVehicleChange::~EnterVehicleChange() {

}

const string& EnterVehicleChange::GetType() const {
	static const string type = "enter_vehicle";
	return type;
}

void EnterVehicleChange::SetVehicle(string vehicle) {
	this->vehicle = vehicle;
}

string EnterVehicleChange::GetVehicle() const {
	return vehicle;
}

LeaveVehicleChange::LeaveVehicleChange() :
	vehicle() {

}

LeaveVehicleChange::LeaveVehicleChange(string vehicle) :
	vehicle(vehicle) {

}

LeaveVehicleChange::~LeaveVehicleChange() {

}

const string& LeaveVehicleChange::GetType() const {
	static const string type = "leave_vehicle";
	return type;
}

void LeaveVehicleChange::SetVehicle(string vehicle) {
	this->vehicle = vehicle;
}

string LeaveVehicleChange::GetVehicle() const {
	return vehicle;
}

CreateTimerChange::CreateTimerChange() :
	name(),
	time(),
	category(),
	label() {

}

CreateTimerChange::CreateTimerChange(string name, string time, string category, string label) :
	name(name),
	time(time),
	category(category),
	label(label) {

}

CreateTimerChange::~CreateTimerChange() {

}

const string& CreateTimerChange::GetType() const {
	static const string type = "create_timer";
	return type;
}

void CreateTimerChange::SetName(string name) {
	this->name = name;
}

string CreateTimerChange::GetName() const {
	return name;
}

void CreateTimerChange::SetTime(string time) {
	this->time = time;
}

string CreateTimerChange::GetTime() const {
	return time;
}

void CreateTimerChange::SetCategory(string category) {
	this->category = category;
}

string CreateTimerChange::GetCategory() const {
	return category;
}

void CreateTimerChange::SetLabel(string label) {
	this->label = label;
}

string CreateTimerChange::GetLabel() const {
	return label;
}

RemoveTimerChange::RemoveTimerChange() :
	name() {

}

RemoveTimerChange::RemoveTimerChange(string name) :
	name(name) {

}

RemoveTimerChange::~RemoveTimerChange() {

}

const string& RemoveTimerChange::GetType() const {
	static const string type = "remove_timer";
	return type;
}

void RemoveTimerChange::SetName(string name) {
	this->name = name;
}

string RemoveTimerChange::GetName() const {
	return name;
}

LaunchElevatorChange::LaunchElevatorChange() :
	building(),
	elevator(),
	command() {

}

LaunchElevatorChange::LaunchElevatorChange(string building, string elevator, string command) :
	building(building), elevator(elevator), command(command) {

}

LaunchElevatorChange::~LaunchElevatorChange() {

}

const string& LaunchElevatorChange::GetType() const {
	static const string type = "launch_elevator";
	return type;
}

void LaunchElevatorChange::SetBuilding(string building) {
	this->building = building;
}

string LaunchElevatorChange::GetBuilding() const {
	return building;
}

void LaunchElevatorChange::SetElevator(string elevator) {
	this->elevator = elevator;
}

string LaunchElevatorChange::GetElevator() const {
	return elevator;
}

void LaunchElevatorChange::SetCommand(string command) {
	this->command = command;
}

string LaunchElevatorChange::GetCommand() const {
	return command;
}

PlayVideoChange::PlayVideoChange() :
	path() {

}

PlayVideoChange::PlayVideoChange(string path) :
	path(path) {

}

PlayVideoChange::~PlayVideoChange() {

}

const string& PlayVideoChange::GetType() const {
	static const string type = "play_video";
	return type;
}

void PlayVideoChange::SetPath(string path) {
	this->path = path;
}

string PlayVideoChange::GetPath() const {
	return path;
}

PlayBgmChange::PlayBgmChange() :
	bgm(),
	loop(true) {

}

PlayBgmChange::PlayBgmChange(string bgm, bool loop) :
	bgm(bgm),
	loop(loop) {

}

PlayBgmChange::~PlayBgmChange() {

}

const string& PlayBgmChange::GetType() const {
	static const string type = "play_bgm";
	return type;
}

void PlayBgmChange::SetBgm(string bgm) {
	this->bgm = bgm;
}

string PlayBgmChange::GetBgm() const {
	return bgm;
}

void PlayBgmChange::SetLoop(bool loop) {
	this->loop = loop;
}

bool PlayBgmChange::GetLoop() const {
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
	name(),
	amount(0) {

}

BankTransactionChange::BankTransactionChange(string name, int amount) :
	name(name), amount(amount) {

}

BankTransactionChange::~BankTransactionChange() {

}

const string& BankTransactionChange::GetType() const {
	static const string type = "bank_transaction";
	return type;
}

void BankTransactionChange::SetName(string name) {
	this->name = name;
}

string BankTransactionChange::GetName() const {
	return name;
}

void BankTransactionChange::SetAmount(int amount) {
	this->amount = amount;
}

int BankTransactionChange::GetAmount() const {
	return amount;
}

GiveEstateChange::GiveEstateChange() :
	estate(),
	name(),
	force(true) {

}

GiveEstateChange::GiveEstateChange(string estate, string name, bool force) :
	estate(estate), name(name), force(force) {

}

GiveEstateChange::~GiveEstateChange() {

}

const string& GiveEstateChange::GetType() const {
	static const string type = "give_estate";
	return type;
}

void GiveEstateChange::SetEstate(string estate) {
	this->estate = estate;
}

string GiveEstateChange::GetEstate() const {
	return estate;
}

void GiveEstateChange::SetName(string name) {
	this->name = name;
}

string GiveEstateChange::GetName() const {
	return name;
}

void GiveEstateChange::SetForce(bool force) {
	this->force = force;
}

bool GiveEstateChange::GetForce() const {
	return force;
}

RemoveEstateChange::RemoveEstateChange() :
	estate(),
	name() {

}

RemoveEstateChange::RemoveEstateChange(string estate, string name) :
	estate(estate), name(name) {

}

RemoveEstateChange::~RemoveEstateChange() {

}

const string& RemoveEstateChange::GetType() const {
	static const string type = "remove_estate";
	return type;
}

void RemoveEstateChange::SetEstate(string estate) {
	this->estate = estate;
}

string RemoveEstateChange::GetEstate() const {
	return estate;
}

void RemoveEstateChange::SetName(string name) {
	this->name = name;
}

string RemoveEstateChange::GetName() const {
	return name;
}

GiveVehicleChange::GiveVehicleChange() :
	vehicle(),
	name(),
	force(true) {

}

GiveVehicleChange::GiveVehicleChange(string vehicle, string name, bool force) :
	vehicle(vehicle), name(name), force(force) {

}

GiveVehicleChange::~GiveVehicleChange() {

}

const string& GiveVehicleChange::GetType() const {
	static const string type = "give_vehicle";
	return type;
}

void GiveVehicleChange::SetVehicle(string vehicle) {
	this->vehicle = vehicle;
}

string GiveVehicleChange::GetVehicle() const {
	return vehicle;
}

void GiveVehicleChange::SetName(string name) {
	this->name = name;
}

string GiveVehicleChange::GetName() const {
	return name;
}

void GiveVehicleChange::SetForce(bool force) {
	this->force = force;
}

bool GiveVehicleChange::GetForce() const {
	return force;
}

RemoveVehicleChange::RemoveVehicleChange() :
	vehicle(),
	name() {

}

RemoveVehicleChange::RemoveVehicleChange(string vehicle, string name) :
	vehicle(vehicle), name(name) {

}

RemoveVehicleChange::~RemoveVehicleChange() {

}

const string& RemoveVehicleChange::GetType() const {
	static const string type = "remove_vehicle";
	return type;
}

void RemoveVehicleChange::SetVehicle(string vehicle) {
	this->vehicle = vehicle;
}

string RemoveVehicleChange::GetVehicle() const {
	return vehicle;
}

void RemoveVehicleChange::SetName(string name) {
	this->name = name;
}

string RemoveVehicleChange::GetName() const {
	return name;
}

GiveObjectChange::GiveObjectChange() :
	object(),
	num(0) {

}

GiveObjectChange::GiveObjectChange(string object, int num) :
	object(object), num(num) {

}

GiveObjectChange::~GiveObjectChange() {

}

const string& GiveObjectChange::GetType() const {
	static const string type = "give_object";
	return type;
}

void GiveObjectChange::SetObject(string object) {
	this->object = object;
}

string GiveObjectChange::GetObject() const {
	return object;
}

void GiveObjectChange::SetNum(int num) {
	this->num = num;
}

int GiveObjectChange::GetNum() const {
	return num;
}

RemoveObjectChange::RemoveObjectChange() :
	object(),
	num(0),
	force(false) {

}

RemoveObjectChange::RemoveObjectChange(string object, int num, bool force) :
	object(object), num(num), force(force) {

}

RemoveObjectChange::~RemoveObjectChange() {

}

const string& RemoveObjectChange::GetType() const {
	static const string type = "remove_object";
	return type;
}

void RemoveObjectChange::SetObject(string object) {
	this->object = object;
}

string RemoveObjectChange::GetObject() const {
	return object;
}

void RemoveObjectChange::SetNum(int num) {
	this->num = num;
}

int RemoveObjectChange::GetNum() const {
	return num;
}

void RemoveObjectChange::SetForce(bool force) {
	this->force = force;
}

bool RemoveObjectChange::GetForce() const {
	return force;
}

PlayerInjuredChange::PlayerInjuredChange() :
	wound() {

}

PlayerInjuredChange::PlayerInjuredChange(string wound) :
	wound(wound) {

}

PlayerInjuredChange::~PlayerInjuredChange() {

}

const string& PlayerInjuredChange::GetType() const {
	static const string type = "player_injured";
	return type;
}

void PlayerInjuredChange::SetWound(string wound) {
	this->wound = wound;
}

string PlayerInjuredChange::GetWound() const {
	return wound;
}

PlayerCuredChange::PlayerCuredChange() :
	wound() {

}

PlayerCuredChange::PlayerCuredChange(string wound) :
	wound(wound) {

}

PlayerCuredChange::~PlayerCuredChange() {

}

const string& PlayerCuredChange::GetType() const {
	static const string type = "player_cured";
	return type;
}

void PlayerCuredChange::SetWound(string wound) {
	this->wound = wound;
}

string PlayerCuredChange::GetWound() const {
	return wound;
}

PlayerIllChange::PlayerIllChange() :
	illness() {

}

PlayerIllChange::PlayerIllChange(string illness) :
	illness(illness) {

}

PlayerIllChange::~PlayerIllChange() {

}

const string& PlayerIllChange::GetType() const {
	static const string type = "player_ill";
	return type;
}

void PlayerIllChange::SetIllness(string illness) {
	this->illness = illness;
}

string PlayerIllChange::GetIllness() const {
	return illness;
}

PlayerRecoverChange::PlayerRecoverChange() :
	illness() {

}

PlayerRecoverChange::PlayerRecoverChange(string illness) :
	illness(illness) {

}

PlayerRecoverChange::~PlayerRecoverChange() {

}

const string& PlayerRecoverChange::GetType() const {
	static const string type = "player_recover";
	return type;
}

void PlayerRecoverChange::SetIllness(string illness) {
	this->illness = illness;
}

string PlayerRecoverChange::GetIllness() const {
	return illness;
}

PlayerSleepChange::PlayerSleepChange() :
	hour(0) {

}

PlayerSleepChange::PlayerSleepChange(int hour) :
	hour(hour) {

}

PlayerSleepChange::~PlayerSleepChange() {

}

const string& PlayerSleepChange::GetType() const {
	static const string type = "player_sleep";
	return type;
}

void PlayerSleepChange::SetHour(int hour) {
	this->hour = hour;
}

int PlayerSleepChange::GetHour() const {
	return hour;
}

ChangeTimeChange::ChangeTimeChange() :
	delta() {

}

ChangeTimeChange::ChangeTimeChange(string delta) :
	delta(delta) {

}

ChangeTimeChange::~ChangeTimeChange() {

}

const string& ChangeTimeChange::GetType() const {
	static const string type = "change_time";
	return type;
}

void ChangeTimeChange::SetDelta(string delta) {
	this->delta = delta;
}

string ChangeTimeChange::GetDelta() const {
	return delta;
}

ChangeCultivationChange::ChangeCultivationChange() :
	method(),
	level(0) {

}

ChangeCultivationChange::ChangeCultivationChange(string method, int level) :
	method(method), level(level) {

}

ChangeCultivationChange::~ChangeCultivationChange() {

}

const string& ChangeCultivationChange::GetType() const {
	static const string type = "change_cultivation";
	return type;
}

void ChangeCultivationChange::SetMethod(string method) {
	this->method = method;
}

string ChangeCultivationChange::GetMethod() const {
	return method;
}

void ChangeCultivationChange::SetLevel(int level) {
	this->level = level;
}

int ChangeCultivationChange::GetLevel() const {
	return level;
}

ChangeWantedChange::ChangeWantedChange() :
	reason(),
	level(0) {

}

ChangeWantedChange::ChangeWantedChange(string reason, int level) :
	reason(reason), level(level) {

}

ChangeWantedChange::~ChangeWantedChange() {

}

const string& ChangeWantedChange::GetType() const {
	static const string type = "change_wanted";
	return type;
}

void ChangeWantedChange::SetReason(string reason) {
	this->reason = reason;
}

string ChangeWantedChange::GetReason() const {
	return reason;
}

void ChangeWantedChange::SetLevel(int level) {
	this->level = level;
}

int ChangeWantedChange::GetLevel() const {
	return level;
}

ChangeWeatherChange::ChangeWeatherChange() :
	weather() {

}

ChangeWeatherChange::ChangeWeatherChange(string weather)
	: weather(weather) {

}

ChangeWeatherChange::~ChangeWeatherChange() {

}

const string& ChangeWeatherChange::GetType() const {
	static const string type = "change_weather";
	return type;
}

void ChangeWeatherChange::SetWeather(string weather) {
	this->weather = weather;
}

string ChangeWeatherChange::GetWeather() const {
	return weather;
}

ChangePolicyChange::ChangePolicyChange() :
	policy() {

}

ChangePolicyChange::ChangePolicyChange(string policy) :
	policy(policy) {

}

ChangePolicyChange::~ChangePolicyChange() {

}

const string& ChangePolicyChange::GetType() const {
	static const string type = "change_policy";
	return type;
}

void ChangePolicyChange::SetPolicy(string policy) {
	this->policy = policy;
}

string ChangePolicyChange::GetPolicy() const {
	return policy;
}
