#include "populace/citizen.h"


using namespace std;

Citizen::Citizen(const string& name, GENDER_TYPE gender, int birthYear, int birthMonth, int birthDay) :
	name(name),
	gender(gender),
	birthYear(birthYear),
	birthMonth(birthMonth),
	birthDay(birthDay) {
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
