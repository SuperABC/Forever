#pragma once

#include "society/job_mod.h"

#include "story/change.h"


class ShopSalerJob : public JobMod {
public:
	ShopSalerJob();

	static const char* GetId() { return "job_shop_saler"; }
	virtual const char* GetType() const override { return "job_shop_saler"; }
	virtual void DailyPlan(const Time& currentTime, PostHandle* post) override;
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

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
