#pragma once

#include "polit/party_mod.h"

#include <string>


// 共和党：主张提高税收和福利、收紧移民，穷人更倾向它，设计见party_republican.md。
class RepublicanParty : public PartyMod {
public:
	FOREVER_MOD_GENERIC_UNIT_TEST("party")
	RepublicanParty();

	static const char* GetId() { return "party_republican"; }
	virtual const char* GetType() const override { return "party_republican"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

	virtual void Propose(const std::vector<PolicyValue>& current) override;
	virtual PartyCongress GetCongress(int year, int month) override;
	virtual void ChooseMeetingRoom(PostHandle* post) override;
	virtual void AssignPowers(const PartyContext& context) override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
