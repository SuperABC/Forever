#include "polit/party.h"

#include <algorithm>


using namespace std;

Party::Party(PartyFactory* factory, PartyMod* mod, const string& id) : mod(mod), factory(factory), id(id) {}

Party::~Party() {
	if (factory && mod) factory->DestroyParty(mod);
}

string Party::GetName() const {
	return mod ? string(mod->GetName()) : string();
}

bool Party::HasMember(const Citizen* citizen) const {
	return find(members.begin(), members.end(), citizen) != members.end();
}
