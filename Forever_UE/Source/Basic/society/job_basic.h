#pragma once

#include "society/job_mod.h"

#include "story/change.h"


// ShopSalerJob：店员，天天上班，早上9点从家出门去商店，中午12点从商店下班回家——这次不要
// Calendar，"哪几天上班/几点上下班"直接写在这里的DailyPlan里。DailyPlan/ExecNode这次
// 完全是C++直接构造Change*，和milestone/JSON无关，见job.md。
class ShopSalerJob : public JobMod {
public:
	static const char* GetId() { return "job_shop_saler"; }
	virtual const char* GetType() const override { return "job_shop_saler"; }
	virtual const char* GetName() override { return "ShopSalerJob"; }

	virtual void DailyPlan(const Time& currentTime) override;
	virtual void ExecNode(const std::string& node) override;
};
