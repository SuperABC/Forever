#pragma once

#include "job_base.h"


// 子类注册函数
typedef void (*RegisterModJobsFunc)(JobFactory* factory);

// 主程序检测子类
class HotelCleanerJob : public Job {
public:
    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

	virtual std::vector<std::string> GetScripts() const override;
};
