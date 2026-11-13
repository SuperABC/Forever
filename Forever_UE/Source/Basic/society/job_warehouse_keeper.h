#pragma once

#include "society/job_mod.h"

#include "story/change.h"


class WarehouseKeeperJob : public JobMod {
public:
	WarehouseKeeperJob();

	static const char* GetId() { return "job_warehouse_keeper"; }
	virtual const char* GetType() const override { return "job_warehouse_keeper"; }
	virtual void DailyPlan(const Time& currentTime, PostHandle* post) override;
	virtual void ExecNode(const std::string& node, PostHandle* post) override;
	virtual const char* GetName() override;
	virtual void SetProperty() override {}

public:
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
