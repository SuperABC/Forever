#pragma once

#include "society/job_mod.h"

#include "story/change.h"


// ShopSalerJob：店员，天天上班，早上9点从家出门去商店，中午12点从商店下班回家——这次不要
// Calendar，"哪几天上班/几点上下班"直接写在这里的DailyPlan里。DailyPlan/ExecNode这次
// 完全是C++直接构造Change*，和milestone/JSON无关，见job.md。构造函数里设
// scriptModName="empty"+milestoneNames={"job_shop_saler"}，让Job::Job()据此创建一个
// 挂了Resource/Story/job_shop_saler.script的Script，见job.md"Script配置"一节。
class ShopSalerJob : public JobMod {
public:
	ShopSalerJob();

	static const char* GetId() { return "job_shop_saler"; }
	virtual const char* GetType() const override { return "job_shop_saler"; }
	// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id，和
	// Source/Basic/map/terrain_basic.h/.cpp的OceanTerrain同一个模式。
	virtual const char* GetName() override;

	virtual void DailyPlan(const Time& currentTime, PostHandle* post) override;
	virtual void ExecNode(const std::string& node, PostHandle* post) override;

private:
	static int count;
	int id;
	std::string name;
};
