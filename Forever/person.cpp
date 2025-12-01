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


