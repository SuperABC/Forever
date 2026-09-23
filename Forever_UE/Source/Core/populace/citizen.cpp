#include "populace/citizen.h"

#include "common/utility.h"
#include "common/error.h"

#include "populace/scheduler.h"
#include "populace/experience.h"

#include <algorithm>


using namespace std;

namespace {
constexpr float kPersonalityStddev = 1.0f / 3.0f; // ±1边界大致对应3σ，落在范围外clamp的比例很小
}

Personality::Personality() :
	appearance(clamp(GetRandomNormal(0.0f, kPersonalityStddev), -1.0f, 1.0f)),
	fitness(clamp(GetRandomNormal(0.0f, kPersonalityStddev), -1.0f, 1.0f)),
	energy(clamp(GetRandomNormal(0.0f, kPersonalityStddev), -1.0f, 1.0f)),
	intelligence(clamp(GetRandomNormal(0.0f, kPersonalityStddev), -1.0f, 1.0f)),
	eloquence(clamp(GetRandomNormal(0.0f, kPersonalityStddev), -1.0f, 1.0f)),
	confidence(clamp(GetRandomNormal(0.0f, kPersonalityStddev), -1.0f, 1.0f)),
	morality(clamp(GetRandomNormal(0.0f, kPersonalityStddev), -1.0f, 1.0f)),
	mentality(clamp(GetRandomNormal(0.0f, kPersonalityStddev), -1.0f, 1.0f)),
	imagination(clamp(GetRandomNormal(0.0f, kPersonalityStddev), -1.0f, 1.0f)),
	knowledge(clamp(GetRandomNormal(0.0f, kPersonalityStddev), -1.0f, 1.0f)),
	art(clamp(GetRandomNormal(0.0f, kPersonalityStddev), -1.0f, 1.0f)),
	reasoning(clamp(GetRandomNormal(0.0f, kPersonalityStddev), -1.0f, 1.0f)),
	perception(clamp(GetRandomNormal(0.0f, kPersonalityStddev), -1.0f, 1.0f)) {
}

float& Personality::operator[](PERSONALITY_TYPE type) {
	switch (type) {
	case PERSONALITY_APPEARANCE: return appearance;
	case PERSONALITY_FITNESS: return fitness;
	case PERSONALITY_ENERGY: return energy;
	case PERSONALITY_INTELLIGENCE: return intelligence;
	case PERSONALITY_ELOQUENCE: return eloquence;
	case PERSONALITY_CONFIDENCE: return confidence;
	case PERSONALITY_MORALITY: return morality;
	case PERSONALITY_MENTALITY: return mentality;
	case PERSONALITY_IMAGINATION: return imagination;
	case PERSONALITY_KNOWLEDGE: return knowledge;
	case PERSONALITY_ART: return art;
	case PERSONALITY_REASONING: return reasoning;
	case PERSONALITY_PERCEPTION: default: return perception;
	}
}

float Personality::operator[](PERSONALITY_TYPE type) const { return const_cast<Personality*>(this)->operator[](type); }

float& Personality::operator()(const string& name) {
	if (name == "appearance") return appearance;
	if (name == "fitness") return fitness;
	if (name == "energy") return energy;
	if (name == "intelligence") return intelligence;
	if (name == "eloquence") return eloquence;
	if (name == "confidence") return confidence;
	if (name == "morality") return morality;
	if (name == "mentality") return mentality;
	if (name == "imagination") return imagination;
	if (name == "knowledge") return knowledge;
	if (name == "art") return art;
	if (name == "reasoning") return reasoning;
	if (name == "perception") return perception;
	THROW_EXCEPTION(InvalidArgumentException, "Unknown personality field " + name + ".\n");
}

float Personality::operator()(const string& name) const { return const_cast<Personality*>(this)->operator()(name); }

string Personality::GetFieldName(PERSONALITY_TYPE type) {
	switch (type) {
	case PERSONALITY_APPEARANCE: return "appearance";
	case PERSONALITY_FITNESS: return "fitness";
	case PERSONALITY_ENERGY: return "energy";
	case PERSONALITY_INTELLIGENCE: return "intelligence";
	case PERSONALITY_ELOQUENCE: return "eloquence";
	case PERSONALITY_CONFIDENCE: return "confidence";
	case PERSONALITY_MORALITY: return "morality";
	case PERSONALITY_MENTALITY: return "mentality";
	case PERSONALITY_IMAGINATION: return "imagination";
	case PERSONALITY_KNOWLEDGE: return "knowledge";
	case PERSONALITY_ART: return "art";
	case PERSONALITY_REASONING: return "reasoning";
	case PERSONALITY_PERCEPTION: default: return "perception";
	}
}

Relation::Relation(RELATIONSHIP_CATEGORY category) :
	category(category),
	familiarity(0.0f), respect(0.0f), favour(0.0f), trust(0.0f), competing(0.0f), reliability(0.0f) {
}

float& Relation::operator[](RELATION_TYPE type) {
	switch (type) {
	case RELATION_FAMILIARITY: return familiarity;
	case RELATION_RESPECT: return respect;
	case RELATION_FAVOUR: return favour;
	case RELATION_TRUST: return trust;
	case RELATION_COMPETING: return competing;
	case RELATION_RELIABILITY: default: return reliability;
	}
}

float Relation::operator[](RELATION_TYPE type) const { return const_cast<Relation*>(this)->operator[](type); }

float& Relation::operator()(const string& name) {
	if (name == "familiarity") return familiarity;
	if (name == "respect") return respect;
	if (name == "favour") return favour;
	if (name == "trust") return trust;
	if (name == "competing") return competing;
	if (name == "reliability") return reliability;
	THROW_EXCEPTION(InvalidArgumentException, "Unknown relation field " + name + ".\n");
}

float Relation::operator()(const string& name) const { return const_cast<Relation*>(this)->operator()(name); }

string Relation::GetFieldName(RELATION_TYPE type) {
	switch (type) {
	case RELATION_FAMILIARITY: return "familiarity";
	case RELATION_RESPECT: return "respect";
	case RELATION_FAVOUR: return "favour";
	case RELATION_TRUST: return "trust";
	case RELATION_COMPETING: return "competing";
	case RELATION_RELIABILITY: default: return "reliability";
	}
}

Citizen::Citizen(const string& name, GENDER_TYPE gender, int birthYear, int birthMonth, int birthDay) :
	name(name),
	gender(gender),
	birthYear(birthYear),
	birthMonth(birthMonth),
	birthDay(birthDay) {
}

Citizen::~Citizen() {
	delete scheduler;
	for (Experience* experience : experiences) {
		delete experience;
	}
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

void Citizen::AddAcquaintance(const string& name, RELATIONSHIP_CATEGORY category) {
	// insert_or_assign而不是acquaintances[name]=...——Relation不再有无参默认构造，
	// operator[]在key不存在时会先默认构造一个值再赋值，编译不过；insert_or_assign直接
	// 就地构造/覆盖，不需要Relation可默认构造。
	acquaintances.insert_or_assign(name, Relation(category));
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

const unordered_map<string, Relation>& Citizen::GetAcquaintances() const { return acquaintances; }

const vector<Experience*>& Citizen::GetExperiences() const { return experiences; }
void Citizen::AddExperience(Experience* experience) { if (experience) experiences.push_back(experience); }

vector<Citizen*> Citizen::GetCurrentLovers() const {
	vector<Citizen*> lovers;
	for (Experience* experience : experiences) {
		if (experience->GetCategory() != RELATIONSHIP_ROMANTIC || !experience->IsOngoing()) continue;
		EmotionExperience* emotion = static_cast<EmotionExperience*>(experience);
		if (emotion->GetOther() != spouse) lovers.push_back(emotion->GetOther());
	}
	return lovers;
}

int Citizen::GetLastGraduationYear() const { return lastGraduationYear; }
void Citizen::SetLastGraduationYear(int year) { lastGraduationYear = year; }

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

const vector<string>& Citizen::GetOptions() const { return options; }
void Citizen::AddOption(const string& option) { options.push_back(option); }
