#include "populace/citizen.h"

#include "common/utility.h"
#include "common/error.h"

#include "populace/scheduler.h"
#include "populace/experience.h"
#include "map/room.h"

#include <algorithm>
#include <cmath>


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
	birthDay(birthDay),
	avatarIndex(GetRandom(kAvatarIndexRange)) {
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

const string& Citizen::GetAvatar() const { return avatar; }
void Citizen::SetAvatar(const string& avatar) { this->avatar = avatar; }
int Citizen::GetAvatarIndex() const { return avatarIndex; }

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
Household* Citizen::GetHousehold() const { return household; }
void Citizen::SetHousehold(Household* value) { household = value; }
void Citizen::SetRoom(Room* value) { room = value; }
Room* Citizen::GetCurrentRoom() const { return currentRoom; }
void Citizen::SetCurrentRoom(Room* value) {
	// 房间的occupants跟着currentRoom走(旧房间移出、新房间加入)。读档直接赋值字段，不经过这里。
	if (currentRoom == value) return;
	if (currentRoom) currentRoom->RemoveOccupant(this);
	currentRoom = value;
	if (currentRoom && !isDead) currentRoom->AddOccupant(this);
}

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

bool Citizen::IsWalking() const { return walkDestination != nullptr; }
Room* Citizen::GetWalkDestination() const { return walkDestination; }

void Citizen::BeginWalk(const std::vector<WalkPoint>& path, Room* destination, double nowSeconds, float speed) {
	EndWalk();
	if (path.empty() || !destination || speed <= 0.f) return;
	walkPath = path;
	walkDistances.assign(path.size(), 0.f);
	for (size_t i = 1; i < path.size(); i++) {
		float dx = path[i].x - path[i - 1].x, dy = path[i].y - path[i - 1].y, dz = path[i].z - path[i - 1].z;
		walkDistances[i] = walkDistances[i - 1] + std::sqrt(dx * dx + dy * dy + dz * dz);
	}
	walkDestination = destination;
	walkStartTime = nowSeconds;
	walkSpeed = speed;
	SetPosition(path[0].x, path[0].y, path[0].z);
}

void Citizen::SampleWalk(double nowSeconds, WalkPoint& outPoint, size_t& outSegment, bool& outFinished) const {
	float travelled = static_cast<float>(std::max(0.0, nowSeconds - walkStartTime)) * walkSpeed;
	if (travelled >= walkDistances.back()) {
		outPoint = walkPath.back();
		outSegment = walkPath.size() - 1;
		outFinished = true;
		return;
	}
	outFinished = false;
	size_t i = 1;
	while (walkDistances[i] < travelled) i++; // travelled < 总弧长，必然在末点之前停下
	float span = walkDistances[i] - walkDistances[i - 1];
	float t = span > 0.f ? (travelled - walkDistances[i - 1]) / span : 0.f;
	outPoint = { walkPath[i - 1].x + (walkPath[i].x - walkPath[i - 1].x) * t,
		walkPath[i - 1].y + (walkPath[i].y - walkPath[i - 1].y) * t,
		walkPath[i - 1].z + (walkPath[i].z - walkPath[i - 1].z) * t };
	outSegment = i;
}

bool Citizen::AdvanceWalk(double nowSeconds) {
	if (!IsWalking()) return false;
	WalkPoint point;
	size_t segment;
	bool finished;
	SampleWalk(nowSeconds, point, segment, finished);
	if (finished) {
		Room* destination = walkDestination;
		EndWalk();
		SetCurrentRoom(destination);
		ClearPosition();
		return true;
	}
	SetPosition(point.x, point.y, point.z);
	return false;
}

void Citizen::GetRemainingWalk(double nowSeconds, std::vector<WalkPoint>& outPath) const {
	outPath.clear();
	if (!IsWalking()) return;
	WalkPoint point;
	size_t segment;
	bool finished;
	SampleWalk(nowSeconds, point, segment, finished);
	outPath.push_back(point);
	for (size_t i = segment; i < walkPath.size() && !finished; i++) outPath.push_back(walkPath[i]);
}

void Citizen::EndWalk() {
	walkDestination = nullptr;
	walkPath.clear();
	walkDistances.clear();
}

Job* Citizen::GetJob() const { return job; }
void Citizen::SetJob(Job* value) { job = value; }

Scheduler* Citizen::GetScheduler() const { return scheduler; }
void Citizen::SetScheduler(Scheduler* value) { scheduler = value; }

const vector<string>& Citizen::GetOptions() const { return options; }
void Citizen::AddOption(const string& option) { options.push_back(option); }
void Citizen::RemoveOption(const string& option) { options.erase(remove(options.begin(), options.end(), option), options.end()); }

float Citizen::GetHealth() const { return health; }
float Citizen::GetMaxHealth() const { return maxHealth; }

float Citizen::TakeDamage(float amount) {
	if (isDead) return 0.f;
	health = max(0.f, health - amount);
	if (health <= 0.f) Die();
	return health;
}

float Citizen::Heal(float amount) {
	if (isDead) return 0.f;
	health = min(maxHealth, health + amount);
	return health;
}

bool Citizen::IsDead() const { return isDead; }

void Citizen::Die() {
	if (isDead) return;
	isDead = true;
	health = 0.f;
	EndWalk();
	SetCurrentRoom(nullptr);
	ClearPosition();
}

void Citizen::Revive(float newHealth) {
	isDead = false;
	health = (newHealth <= 0.f || newHealth > maxHealth) ? maxHealth : newHealth;
}
