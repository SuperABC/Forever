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
	// reserve理论上不可能覆盖整个姓名空间，但要防御性地避免极端情况死循环——重试到这个
	// 上限还是撞上reserve里的名字，视为致命错误（reserve集合相对mod能生成的姓名空间
	// 大到不正常的地步，属于配置问题），直接抛异常，交给AForeverFrameworkActor::
	// BeginPlay()的try/catch统一处理（打日志+退出游戏），不能静默返回一个撞名的结果，
	// 见populace.md"InitNames"一节。
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
