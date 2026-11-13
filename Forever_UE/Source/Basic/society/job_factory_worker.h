#pragma once

#include "society/job_mod.h"

#include "story/change.h"


class FactoryWorkerJob : public JobMod {
public:
	FactoryWorkerJob();

	static const char* GetId() { return "job_factory_worker"; }
	virtual const char* GetType() const override { return "job_factory_worker"; }
	virtual void DailyPlan(const Time& currentTime, PostHandle* post) override;
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
