#pragma once

#include "society/job_mod.h"

#include "story/change.h"


// ShopSalerJob：店员，天天上班，早上9点从家出门去商店，晚上18点从商店下班回家。上下班由
// Resource/Story/job_shop_saler.script里的计时器+npc_navigate驱动(脚本里用$$self.occupant/
// home/workplace，见job.md"脚本驱动通勤")，这里的DailyPlan不排任何C++节点。SetProperty()里设
// scriptModName="script_empty"+milestoneNames={"job_shop_saler"}，见job.md"Script配置"一节。

class ShopSalerJob : public JobMod {
public:
	ShopSalerJob();

	static const char* GetId() { return "job_shop_saler"; }
	virtual const char* GetType() const override { return "job_shop_saler"; }
	virtual void DailyPlan(const Time& currentTime, PostHandle* post) override;
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

// WarehouseKeeperJob：商店仓库房间的管理员，没有剧情脚本，9点上班18点下班。
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

// FactoryWorkerJob：工坊房间的工人，没有剧情脚本，8点上班17点下班。
class FactoryWorkerJob : public JobMod {
public:
	FactoryWorkerJob();

	static const char* GetId() { return "job_factory_worker"; }
	virtual const char* GetType() const override { return "job_factory_worker"; }
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
