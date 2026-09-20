#include "name.h"

#include "common/error.h"


using namespace std;

Name::Name(NameFactory* factory, const string& nameId) :
	mod(factory->CreateName(nameId)),
	factory(factory),
	type(),
	name() {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "Name " + nameId + " mod is null.\n");
	}

	type = mod->GetType();
	name = mod->GetName();
}

Name::~Name() {
	factory->DestroyName(mod);
}

string Name::GetType() const {
	return type;
}

string Name::GetName() const {
	return name;
}

NameMod* Name::GetMod() const {
	return mod;
}

string Name::GetSurname(const string& fullName) const {
	string result;
	mod->GetSurname(fullName, [&](const string& value) { result = value; });
	return result;
}

string Name::GenerateName(bool allowMale, bool allowFemale, bool allowNeutral) const {
	string result;
	mod->GenerateName(allowMale, allowFemale, allowNeutral, [&](const string& value) { result = value; });
	return result;
}

string Name::GenerateName(const string& surname,
	bool allowMale, bool allowFemale, bool allowNeutral) const {
	string result;
	mod->GenerateName(surname, allowMale, allowFemale, allowNeutral, [&](const string& value) { result = value; });
	return result;
}
