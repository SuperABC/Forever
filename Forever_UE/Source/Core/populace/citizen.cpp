#include "populace/citizen.h"

#include "common/utility.h"
#include "common/error.h"

#include "populace/scheduler.h"

#include <algorithm>


using namespace std;

Citizen::Citizen(const string& name, GENDER_TYPE gender, int birthYear, int birthMonth, int birthDay) :
	name(name),
	gender(gender),
	birthYear(birthYear),
	birthMonth(birthMonth),
	birthDay(birthDay) {
}

Citizen::~Citizen() {
	delete scheduler;
}

const string& Citizen::GetName() const { return name; }
GENDER_TYPE Citizen::GetGender() const { return gender; }
int Citizen::GetBirthYear() const { return birthYear; }
int Citizen::GetBirthMonth() const { return birthMonth; }
int Citizen::GetBirthDay() const { return birthDay; }

int Citizen::GetAge(int currentYear) const { return currentYear - birthYear; }

Citizen* Citizen::GetSpouse() const { return spouse; }
void Citizen::SetSpouse(Citizen* value) { spouse = value; }
const vector<Citizen*>& Citizen::GetChildren() const { return children; }
void Citizen::AddChild(Citizen* child) { if (child) children.push_back(child); }

const Personality& Citizen::GetPersonality() const { return personality; }
void Citizen::SetPersonalityValue(PERSONALITY_TYPE type, float value) { personality[type] = value; }
void Citizen::AdjustPersonalityValue(PERSONALITY_TYPE type, float delta) { personality[type] = clamp(personality[type] + delta, 0.0f, 1.0f); }

void Citizen::AddAcquaintance(const string& name) {
	acquaintances[name] = Relation();
	SetAcquaintanceValue(name, RELATION_FAMILIARITY, acquaintances[name][RELATION_FAMILIARITY]);
	SetAcquaintanceValue(name, RELATION_RESPECT, acquaintances[name][RELATION_RESPECT]);
	SetAcquaintanceValue(name, RELATION_FAVOUR, acquaintances[name][RELATION_FAVOUR]);
	SetAcquaintanceValue(name, RELATION_TRUST, acquaintances[name][RELATION_TRUST]);
	SetAcquaintanceValue(name, RELATION_COMPETING, acquaintances[name][RELATION_COMPETING]);
	SetAcquaintanceValue(name, RELATION_RELIABILITY, acquaintances[name][RELATION_RELIABILITY]);
}

const Relation& Citizen::GetAcquaintance(const string& name) const {
	auto it = acquaintances.find(name);
	if (it == acquaintances.end()) {
		THROW_EXCEPTION(InvalidArgumentException, "Acquaintance " + name + " not found.\n");
	}
	return it->second;
}

void Citizen::SetAcquaintanceValue(const string& name, RELATION_TYPE type, float value) {
	auto it = acquaintances.find(name);
	if (it == acquaintances.end()) {
		THROW_EXCEPTION(InvalidArgumentException, "Acquaintance " + name + " not found.\n");
	}
	it->second[type] = value;
}

void Citizen::AdjustAcquaintanceValue(const string& name, RELATION_TYPE type, float delta) {
	auto it = acquaintances.find(name);
	if (it == acquaintances.end()) {
		THROW_EXCEPTION(InvalidArgumentException, "Acquaintance " + name + " not found.\n");
	}
	float value = clamp(it->second[type] + delta, 0.0f, 1.0f);
	it->second[type] = value;
}

Lot* Citizen::GetLot() const { return lot; }
void Citizen::SetLot(Lot* value) { lot = value; }
Zone* Citizen::GetZone() const { return zone; }
void Citizen::SetZone(Zone* value) { zone = value; }
Building* Citizen::GetBuilding() const { return building; }
void Citizen::SetBuilding(Building* value) { building = value; }
Room* Citizen::GetRoom() const { return room; }
void Citizen::SetRoom(Room* value) { room = value; }
Room* Citizen::GetCurrentRoom() const { return currentRoom; }
void Citizen::SetCurrentRoom(Room* value) { currentRoom = value; }

bool Citizen::HasPosition() const { return hasPosition; }

void Citizen::GetPosition(float& outX, float& outY, float& outZ) const {
	outX = posX;
	outY = posY;
	outZ = posZ;
}

void Citizen::SetPosition(float x, float y, float z) {
	posX = x;
	posY = y;
	posZ = z;
	hasPosition = true;
}

void Citizen::ClearPosition() { hasPosition = false; }

Job* Citizen::GetJob() const { return job; }
void Citizen::SetJob(Job* value) { job = value; }

Scheduler* Citizen::GetScheduler() const { return scheduler; }
void Citizen::SetScheduler(Scheduler* value) { scheduler = value; }
