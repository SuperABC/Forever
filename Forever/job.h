#pragma once

#include "job_base.h"


// 子类注册函数
typedef void (*RegisterModJobsFunc)(JobFactory* factory);

// 主程序检测子类
class TestJob : public Job {
public:
    static std::string GetId() { return "test"; }
    virtual std::string GetType() const override { return "test"; }
    virtual std::string GetName() const override { return "测试职业"; }

	virtual std::vector<std::string> GetScripts() const override;
};
