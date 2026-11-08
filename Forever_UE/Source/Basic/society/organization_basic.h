#pragma once

#include "society/organization_mod.h"


class ShopOrganization : public OrganizationMod {
public:
	ShopOrganization();

	static const char* GetId() { return "organization_shop"; }
	virtual const char* GetType() const override { return "organization_shop"; }
	virtual const char* GetName() override;

	static float GetPower() { return 1.f; }
	static const std::unordered_map<std::string, std::pair<int, int>>& GetRequirements();

	virtual void DesignJobsForRoom(const std::string& componentType, const std::string& componentName,
		const std::string& roomType, const std::string& roomName, int workspaceCapacity, bool reset) override;
	virtual void MonthlySettle(const OrganizationMonthlyState& state) override;
	virtual void CollectServices(Container* script, const std::vector<std::string>& stockTypes,
		PostHandle* post) override;
	virtual void ApplyServiceUsage(const std::string& name, int count, Container* script) override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class FactoryOrganization : public OrganizationMod {
public:
	FactoryOrganization();

	static const char* GetId() { return "organization_factory"; }
	virtual const char* GetType() const override { return "organization_factory"; }
	virtual const char* GetName() override;

	static float GetPower() { return 1.f; }
	static const std::unordered_map<std::string, std::pair<int, int>>& GetRequirements();

	virtual void DesignJobsForRoom(const std::string& componentType, const std::string& componentName,
		const std::string& roomType, const std::string& roomName, int workspaceCapacity, bool reset) override;
	virtual void MonthlySettle(const OrganizationMonthlyState& state) override;

public:
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
