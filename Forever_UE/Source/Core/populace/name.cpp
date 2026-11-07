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

namespace {
	// 重试到上限仍撞上reserve视为致命配置错误，抛异常，不静默返回撞名结果，见name.md。
	constexpr int kMaxReserveRetryAttempts = 1000;
}

string Name::GenerateName(bool allowMale, bool allowFemale, bool allowNeutral) const {
	string result;
	for (int attempt = 0; attempt < kMaxReserveRetryAttempts; attempt++) {
		mod->GenerateName(allowMale, allowFemale, allowNeutral, [&](const string& value) { result = value; });
		if (result.empty() || reserve.find(result) == reserve.end()) return result;
	}
	THROW_EXCEPTION(DeadLoopException,
		"Name generator could not avoid reserved names after " + to_string(kMaxReserveRetryAttempts) + " attempts.\n");
}

string Name::GenerateName(const string& surname,
	bool allowMale, bool allowFemale, bool allowNeutral) const {
	string result;
	for (int attempt = 0; attempt < kMaxReserveRetryAttempts; attempt++) {
		mod->GenerateName(surname, allowMale, allowFemale, allowNeutral, [&](const string& value) { result = value; });
		if (result.empty() || reserve.find(result) == reserve.end()) return result;
	}
	THROW_EXCEPTION(DeadLoopException,
		"Name generator could not avoid reserved names after " + to_string(kMaxReserveRetryAttempts) + " attempts.\n");
}

void Name::ReserveName(const string& value) {
	reserve.insert(value);
}
