#pragma once

#include "polit/party_mod.h"

#include <string>


// 民主党：主张降低税收和福利、放宽移民，富人更倾向它，设计见party_democratic.md。
class DemocraticParty : public PartyMod {
public:
	FOREVER_MOD_GENERIC_UNIT_TEST("party")
	DemocraticParty();

	static const char* GetId() { return "party_democratic"; }
	virtual const char* GetType() const override { return "party_democratic"; }
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
