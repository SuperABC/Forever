#include "populace/household.h"

#include "populace/citizen.h"

#include <algorithm>


using namespace std;

namespace {
	bool Related(const Citizen* a, const Citizen* b) {
		if (!a || !b) return false;
		if (a->GetSpouse() == b || b->GetSpouse() == a) return true;
		for (const Citizen* child : a->GetChildren()) {
			if (child == b) return true;
		}
		for (const Citizen* child : b->GetChildren()) {
			if (child == a) return true;
		}
		return false;
	}
}

Household::Household(Room* room) : room(room) {
}

Room* Household::GetRoom() const { return room; }

const vector<Citizen*>& Household::GetMembers() const { return members; }

vector<Citizen*> Household::GetAdults(int currentYear) const {
	vector<Citizen*> adults;
	for (Citizen* member : members) {
		if (member && member->GetAge(currentYear) >= 18) adults.push_back(member);
	}
	return adults;
}

bool Household::Contains(const Citizen* citizen) const {
	return find(members.begin(), members.end(), citizen) != members.end();
}

bool Household::IsRelatedTo(const Citizen* citizen) const {
	for (const Citizen* member : members) {
		if (Related(member, citizen)) return true;
	}
	return false;
}

void Household::AddMember(Citizen* citizen) {
	if (citizen && !Contains(citizen)) members.push_back(citizen);
}

void Household::Absorb(Household& other) {
	for (Citizen* member : other.members) AddMember(member);
	other.members.clear();
}
