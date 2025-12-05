#include "person.h"


using namespace std;

Person::Person() {

}

Person::~Person() {

}

int Person::GetId() const {
	return id;
}

void Person::SetId(int id) {
	this->id = id;
}

const string& Person::GetName() const {
	return name;
}

void Person::SetName(const string& name) {
	this->name = name;
}

GENDER_TYPE Person::GetGender() const {
	return gender;
}

void Person::SetGender(GENDER_TYPE gender) {
	this->gender = gender;
}

int Person::GetHeight() const {
	return height;
}

void Person::SetHeight(int height) {
	this->height = height;
}

int Person::GetWeight() const {
	return weight;
}

void Person::SetWeight(int weight) {
	this->weight = weight;
}

const Time& Person::GetBirthday() const {
	return birthday;
}

void Person::SetBirthday(const Time& birthday) {
	this->birthday = birthday;
}

int Person::GetAge(const Time& currentTime) const {
	int age = currentTime.GetYear() - birthday.GetYear();
	if (currentTime.GetMonth() < birthday.GetMonth() ||
		(currentTime.GetMonth() == birthday.GetMonth() && currentTime.GetDay() < birthday.GetDay())) {
		age--;
	}
	return age;
}

const Time& Person::GetMarryday() const {
	return marryday;
}

void Person::SetMarryday(const Time& marryday) {
	this->marryday = marryday;
}

const string& Person::GetNick() const {
	return nick;
}

void Person::SetNick(const string& nick) {
	this->nick = nick;
}

int Person::GetDeposit() const {
	return deposit;
}

void Person::SetDeposit(int deposit) {
	this->deposit = deposit;
}

int Person::GetSimulate() const {
	return simulate;
}

void Person::SetSimulate(int simulate) {
	this->simulate = simulate;
}

void Person::AddRelative(RELATIVE_TYPE type, shared_ptr<Person> person) {
	relatives.push_back(make_pair(type, person));
}

shared_ptr<Person> Person::GetFather() {
	for (auto relative : relatives) {
		if (relative.first == RELATIVE_FATHER)return relative.second;
	}
	return nullptr;
}

shared_ptr<Person> Person::GetMother() {
	for (auto relative : relatives) {
		if (relative.first == RELATIVE_MOTHER)return relative.second;
	}
	return nullptr;
}

shared_ptr<Person> Person::GetSpouse() {
	for (auto relative : relatives) {
		if (relative.first == RELATIVE_WIFE || relative.first == RELATIVE_HUSBAND)return relative.second;
	}

	return nullptr;
}

vector<shared_ptr<Person>> Person::GetChilds() {
	vector<shared_ptr<Person>>childs;
	for (auto relative : relatives) {
		if (relative.first == RELATIVE_SON || relative.first == RELATIVE_DAUGHTER)childs.push_back(relative.second);
	}
	return childs;
}

void Person::AddAsset(std::shared_ptr<Asset> asset) {
	assets.push_back(asset);
}

std::vector<std::shared_ptr<Asset>>& Person::GetAssets() {
	return assets;
}

std::vector<std::shared_ptr<Asset>> Person::GetAssets(std::string type) {
	std::vector<std::shared_ptr<Asset>> results;

	for (auto& asset : assets) {
		if (asset->GetType() == type) {
			results.push_back(asset);
		}
	}

	return results;
}

std::shared_ptr<Asset> Person::GetAsset(std::string name) {
	for (auto& asset : assets) {
		if (asset->GetName() == name) {
			return asset;
		}
	}

	return nullptr;
}

vector<shared_ptr<Job>> Person::GetJobs() {
	return jobs;
}

void Person::AddJob(shared_ptr<Job> job) {
	jobs.push_back(job);
}

void Person::RemoveJob(std::shared_ptr<Job> job) {
	for (auto &j : jobs) {
		if (j == job) {
			j = *jobs.end();
			jobs.pop_back();
			break;
		}
	}
}

shared_ptr<Room> Person::GetHome() {
	return home;
}

void Person::SetHome(std::shared_ptr<Room> room) {
	home = room;
}

void Person::RemoveHome() {
	home = nullptr;
}

shared_ptr<Scheduler> Person::GetScheduler() {
	return scheduler;
}

void Person::SetScheduler(std::shared_ptr<Scheduler> scheduler) {
	this->scheduler = scheduler;
}

