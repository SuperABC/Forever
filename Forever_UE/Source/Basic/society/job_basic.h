#pragma once

#include "society/job_mod.h"

#include "story/change.h"


// CommuteJob：上下班通勤的岗位基类，9:00出门去工位、12:00回家，DailyPlan/ExecNode是C++直接构造
// NPCNavigateChange，和milestone/JSON无关，见job.md。具体岗位只决定自己的id和脚本。
class CommuteJob : public JobMod {
public:
	virtual void DailyPlan(const Time& currentTime, PostHandle* post) override;
	virtual void ExecNode(const std::string& node, PostHandle* post) override;
};

// ShopSalerJob：店员，天天上班，早上9点从家出门去商店，中午12点从商店下班回家——这次不要
// Calendar，"哪几天上班/几点上下班"直接写在这里的DailyPlan里。DailyPlan/ExecNode这次
// 完全是C++直接构造Change*，和milestone/JSON无关，见job.md。SetProperty()里设
// scriptModName="empty"+milestoneNames={"job_shop_saler"}，让Job::Job()据此创建一个
// 挂了Resource/Story/job_shop_saler.script的Script，见job.md"Script配置"一节。

class ShopSalerJob : public CommuteJob {
public:
	ShopSalerJob();

	static const char* GetId() { return "job_shop_saler"; }
	virtual const char* GetType() const override { return "job_shop_saler"; }
	// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id，和
	// Source/Basic/map/terrain_basic.h/.cpp的OceanTerrain同一个模式。
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

// WarehouseKeeperJob：商店仓库房间的管理员，没有剧情脚本。
class WarehouseKeeperJob : public CommuteJob {
public:
	WarehouseKeeperJob();

	static const char* GetId() { return "job_warehouse_keeper"; }
	virtual const char* GetType() const override { return "job_warehouse_keeper"; }
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

// FactoryWorkerJob：工坊房间的工人，没有剧情脚本。
class FactoryWorkerJob : public CommuteJob {
public:
	FactoryWorkerJob();

	static const char* GetId() { return "job_factory_worker"; }
	virtual const char* GetType() const override { return "job_factory_worker"; }
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
